/* This file is part of the Marble Marcher (https://github.com/HackerPoet/MarbleMarcher).
* Copyright(C) 2018 CodeParade
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.If not, see <http://www.gnu.org/licenses/>.
*/

#include<Gamemodes.h>
#include "Level.h"
#include "Res.h"
#include "SelectRes.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <AntTweakBar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>





#ifdef _WIN32
#include <Windows.h>
#define ERROR_MSG(x) MessageBox(nullptr, TEXT(x), TEXT("ERROR"), MB_OK);
#else
#define ERROR_MSG(x) std::cerr << x << std::endl;
#endif

//Graphics settings
static bool VSYNC = true;


#if defined(_WIN32)
int WinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow) {
#else
int main(int argc, char *argv[]) {
#endif
  //Make sure shader is supported
	std::stringstream error_log;
	sf::err().rdbuf(error_log.rdbuf());
  if (!sf::Shader::isAvailable()) {
    ERROR_MSG((error_log.str()).c_str());
    return 1;
  }
  //Load the vertex shader
  sf::Shader shader;
  if (!shader.loadFromFile(vert_glsl, sf::Shader::Vertex)) {
    ERROR_MSG((error_log.str()).c_str());
    return 1;
  }
  //Load the fragment shader
  if (!shader.loadFromFile(frag_glsl, sf::Shader::Fragment)) {
    ERROR_MSG((error_log.str()).c_str());
    return 1;
  }

  //Load the font
  sf::Font font;
  if (!font.loadFromFile(Orbitron_Bold_ttf)) {
    ERROR_MSG("Unable to load font");
    return 1;
  }
  //Load the mono font
  sf::Font font_mono;
  if (!font_mono.loadFromFile(Inconsolata_Bold_ttf)) {
    ERROR_MSG("Unable to load mono font");
    return 1;
  }

  //Load the music
  sf::Music menu_music;
  menu_music.openFromFile(menu_ogg);
  menu_music.setLoop(true);
  sf::Music level_music[num_level_music];
  level_music[0].openFromFile(level1_ogg);
  level_music[0].setLoop(true);
  level_music[1].openFromFile(level2_ogg);
  level_music[1].setLoop(true);
  level_music[2].openFromFile(level3_ogg);
  level_music[2].setLoop(true);
  level_music[3].openFromFile(level4_ogg);
  level_music[3].setLoop(true);
  sf::Music credits_music;
  credits_music.openFromFile(credits_ogg);
  credits_music.setLoop(true);

  //Get the directory for saving and loading high scores
#ifdef _WIN32
  const std::string save_dir = std::string(std::getenv("APPDATA")) + "\\MarbleMarcher";
#else
  const std::string save_dir = std::string(std::getenv("HOME")) + "/.MarbleMarcher";
#endif
  
  if (!DirExists(save_dir.c_str())) {
#if defined(_WIN32)
    bool success = CreateDirectory(save_dir.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    bool success = mkdir(save_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
    if (!success) {
      ERROR_MSG("Failed to create save directory");
      return 1;
    }
  }
  const std::string save_file = save_dir + "/scores.bin";
  const std::string settings_file = save_dir + "/settings.bin";

  game_settings.Load(settings_file);

  //Have user select the resolution
  SelectRes select_res(&font_mono);
  const Resolution* resolution = select_res.Run();
  bool fullscreen = select_res.FullScreen();
  if (resolution == nullptr) {
    return 0;
  }

  //GL settings
  sf::ContextSettings settings;
  settings.majorVersion = 4;
  settings.minorVersion = 3;

  //Create the window
  sf::VideoMode screen_size;
  sf::Uint32 window_style;
  const sf::Vector2i screen_center(resolution->width / 2, resolution->height / 2);
  if (fullscreen) {
    screen_size = sf::VideoMode::getDesktopMode();
    window_style = sf::Style::Fullscreen;
  } else {
    screen_size = sf::VideoMode(resolution->width, resolution->height, 24);
  	window_style = sf::Style::Default;
  }

  AdditionalSettings addsett;
  addsett.Load("assets/config.txt");
  LOCAL.LoadLocalsFromFolder(local_folder);
  LOCAL.SetLanguage(addsett.lang);

  sf::Vector2f screenshot_size = sf::Vector2f(addsett.screenshot_width, addsett.screenshot_height);

  sf::RenderWindow window(screen_size, "Marble Marcher Community Edition", window_style, settings);
  window.setVerticalSyncEnabled(VSYNC);
  window.setKeyRepeatEnabled(false);

  INIT();

  sf::VideoMode fs_size = sf::VideoMode::getDesktopMode();
  window.setSize(sf::Vector2u(fs_size.width, fs_size.width*9.f/16.f));
  window.setPosition(sf::Vector2i(0, 0));
  window.requestFocus();
  UpdateAspectRatio(window.getSize().x, window.getSize().y);
  //set window icon
  sf::Image icon;
  icon.loadFromFile(icon_png); 
  window.setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());

  //If native resolution is the same, then we don't need a render texture
  if (resolution->width == screen_size.width && resolution->height == screen_size.height) {
    fullscreen = false;
  }

  //force fullscreen mode
  fullscreen = true;

  Renderer rend(resolution->width, resolution->height, "shaders/compute/MAIN.cfg");

  //fullscreen = false;
  //Create the render texture if needed
  sf::RenderTexture renderTexture;
  //screenshot number
  int screenshot_i = 0;
  sf::RenderTexture screenshotTexture;

  //new rendering textures
  sf::Image mimg, simg;
  mimg.create(resolution->width, resolution->height, sf::Color::Blue);
  sf::Texture main_txt, screenshot_txt;
  main_txt.loadFromImage(mimg);
  screenshot_txt.create(screenshot_size.x, screenshot_size.y);

  screenshotTexture.create(screenshot_size.x, screenshot_size.y, settings);
  screenshotTexture.setSmooth(false);

  if (fullscreen) {
    renderTexture.create(resolution->width, resolution->height, settings);
    renderTexture.setSmooth(false);
  }

  sf::View default_window_view = window.getDefaultView();

  //Create the fractal scene
  Scene scene(level_music);
  const sf::Glsl::Vec2 window_res((float)resolution->width, (float)resolution->height);
  const sf::Glsl::Vec2 sres_res((float)screenshot_size.x, (float)screenshot_size.y);
  scene.Write(shader);
  scene.SetResolution(shader, window_res.x, window_res.y);
  scene.SetWindowResolution(window.getSize().x, window.getSize().y);
  //Create screen rectangle
  sf::RectangleShape rect;
  rect.setSize(window_res);
  rect.setPosition(0, 0);

  sf::RectangleShape rect_scrshot;
  rect_scrshot.setSize(sres_res);
  rect_scrshot.setPosition(0, 0);
  
  menu_music.setVolume(GetVol());
  //menu_music.play();

  //Main loop
  sf::Clock clock;
  float smooth_fps = target_fps;
  float lag_ms = 0.0f;
  mouse_pos = sf::Vector2i(0, 0);
  mouse_prev_pos = sf::Vector2i(0, 0);

  //temporary level generation code
  /*for (int i = 0; i < 24; i++)
  {
	  all_levels[i].desc = "Official Level by Codeparade";
	  all_levels[i].SaveToFile(std::string(level_folder) + "/" + ConvertSpaces2_(all_levels[i].txt)+".lvl", i, (i<24)?(i+1):-1);
  }*/

  scene.levels.LoadLevelsFromFolder(level_folder);
  scene.levels.LoadMusicFromFolder(music_folder);

  //Create the menus
  Overlays overlays(&font, &font_mono, &scene);
  overlays.SetScale(float(screen_size.width) / 1280.0f);
 

  scene.StartDefault();
  overlays.SetAntTweakBar(window.getSize().x, window.getSize().y, smooth_fps, &scene, &rend, &VSYNC, &mouse_sensitivity, &wheel_sensitivity, &music_vol, &target_fps);
  
  io_state.window_size = sf::Vector2f(window.getSize().x, window.getSize().y);
  float prev_s = 0;

  OpenMainMenu(&scene, &overlays);

  while (window.isOpen()) {
    sf::Event event;
	
	float mouse_wheel = 0.0f;
	mouse_prev_pos = mouse_pos;
	io_state.mouse_prev = sf::Vector2f(mouse_prev_pos.x, mouse_prev_pos.y);
	io_state.wheel = mouse_wheel;
	io_state.mouse_press[2] = false;
	io_state.mouse_press[0] = false;
	while (window.pollEvent(event)) 
	{
		bool handled = overlays.TwManageEvent(&event);
		if (event.type == sf::Event::Closed) {
			window.close();
			break;
		}
		else if (event.type == sf::Event::LostFocus) {
			if (game_mode == PLAYING) {
				PauseGame(window, scene);
			}
		}
		else if (event.type == sf::Event::Resized) {
			screen_size.width = event.size.width;
			screen_size.height = event.size.height;
			overlays.SetScale( std::max(float(screen_size.width), float(screen_size.height))/ 1280.0f);
			sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
			default_window_view = sf::View(visibleArea);
			window.setView(default_window_view);
			io_state.window_size = sf::Vector2f(window.getSize().x, window.getSize().y);
			UpdateAspectRatio(window.getSize().x, window.getSize().y);
			scene.SetWindowResolution(window.getSize().x, window.getSize().y);
			rend.camera.SetAspectRatio((float)window.getSize().x / (float)window.getSize().y);
		}

		// If event has not been handled by AntTweakBar, process it
		if (!handled)
		{
			if (event.type == sf::Event::KeyPressed) {
				const sf::Keyboard::Key keycode = event.key.code;
				all_keys[keycode] = true;
				io_state.keys[keycode] = true;
				if (event.key.code < 0 || event.key.code >= sf::Keyboard::KeyCount) { continue; }
				if (game_mode == CREDITS) {
					OpenMainMenu(&scene, &overlays);
					UnlockMouse(window);
					scene.SetMode(Scene::INTRO);
					scene.SetExposure(1.0f);
					credits_music.stop();
					scene.levels.StopAllMusic();
					menu_music.setVolume(GetVol());
					//menu_music.play();
				}
				else if (game_mode == MIDPOINT) {
					game_mode = PLAYING;
					scene.SetExposure(1.0f);
					credits_music.stop();
					scene.StartNextLevel();
				}
				else if (keycode == sf::Keyboard::Escape) {
					if (game_mode == MAIN_MENU) {
						window.close();
						break;
					}
					else if (game_mode == CONTROLS || game_mode == LEVELS) {
						OpenMainMenu(&scene, &overlays);
					}
					else if (game_mode == SCREEN_SAVER) {
						OpenMainMenu(&scene, &overlays);
					}
					else if (game_mode == PAUSED) {
						game_mode = PLAYING;
						scene.GetCurMusic().setVolume(GetVol());
						scene.SetExposure(1.0f);
						LockMouse(window);
					}
					else if (game_mode == PLAYING) {
						PauseGame(window, scene);
					}
					else if (game_mode == LEVEL_EDITOR)
					{
						RemoveGlobalObject(focused);
						ConfirmEditorExit(&scene, &overlays);
					}
				}
				else if (keycode == sf::Keyboard::R) {
					if (game_mode == PLAYING) {
						scene.ResetLevel();
					}
				}
				else if (keycode == sf::Keyboard::F1) {
					if (game_mode == PLAYING) {
						show_cheats = !show_cheats;
						scene.EnbaleCheats();
					}
				} else if (keycode == sf::Keyboard::F5) { 
					///Screenshot
					screenshot_i++;
					//Update the shader values
					scene.SetResolution(shader, sres_res.x, sres_res.y);
					rend.ReInitialize(sres_res.x, sres_res.y);
					scene.WriteRenderer(rend);
					shader.setUniform("render_texture", screenshot_txt);
					rend.SetOutputTexture(screenshot_txt);
					scene.Write(shader);

					//Setup full-screen shader
					sf::RenderStates states = sf::RenderStates::Default;
					states.shader = &shader;
					//shader.setUniform("render_texture", screenshot_txt);

					window.setActive(false);
					//Draw the fractal
					//Draw to the render texture
					screenshotTexture.setActive(true);

					rend.camera.SetMotionBlur(0);
					rend.Render();

					screenshotTexture.draw(rect_scrshot, states);
					screenshotTexture.display();
					screenshotTexture.getTexture().copyToImage().saveToFile((std::string)"screenshots/screenshot"+(std::string)num2str(time(NULL))+".jpg");

					screenshotTexture.setActive(false);
					window.setActive(true);

					scene.SetResolution(shader, window_res.x, window_res.y);
					rend.ReInitialize(window_res.x, window_res.y);
					rend.camera.SetAspectRatio((float)window.getSize().x / (float)window.getSize().y);
					scene.Write(shader);
			    } else if (keycode == sf::Keyboard::F4) {
					overlays.TWBAR_ENABLED = !overlays.TWBAR_ENABLED;
				} 	else if (keycode == sf::Keyboard::C) {
					scene.Cheat_ColorChange();
				} 	else if (keycode == sf::Keyboard::F) {
					scene.Cheat_FreeCamera();
				}
				else if (keycode == sf::Keyboard::G) {
					scene.Cheat_Gravity();
				}
				else if (keycode == sf::Keyboard::H) {
					scene.Cheat_HyperSpeed();
				}
				else if (keycode == sf::Keyboard::I) {
					scene.Cheat_IgnoreGoal();
				}
				else if (keycode == sf::Keyboard::M) {
					scene.Cheat_Motion();
				}
				else if (keycode == sf::Keyboard::P) {
					scene.Cheat_Planet();
				}
				else if (keycode == sf::Keyboard::Z) {
					if (scene.GetParamMod() == -1) {
						scene.Cheat_Zoom();
					}
					else {
						scene.Cheat_Param(-1);
					}
				} if (keycode >= sf::Keyboard::Num0 && keycode <= sf::Keyboard::Num9) {
					scene.Cheat_Param(int(keycode) - int(sf::Keyboard::Num1));
				}
				
			}
			else if (event.type == sf::Event::KeyReleased) {
				const sf::Keyboard::Key keycode = event.key.code;
				if (event.key.code < 0 || event.key.code >= sf::Keyboard::KeyCount) { continue; }
				all_keys[keycode] = false;
				io_state.keys[keycode] = false;
			}
			else if (event.type == sf::Event::MouseButtonPressed) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					mouse_pos = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
					mouse_clicked = true;
					io_state.mouse[0] = true;
					io_state.mouse_press[0] = true;
					if (game_mode == MAIN_MENU) {
						const Overlays::Texts selected = overlays.GetOption(Overlays::PLAY, Overlays::EXIT);
					}
					else if (game_mode == CONTROLS) {
						const Overlays::Texts selected = overlays.GetOption(Overlays::BACK, Overlays::BACK);
						if (selected == Overlays::BACK) {
							OpenMainMenu(&scene, &overlays);
						}
					}
					else if (game_mode == LEVELS) {
					
					}
					else if (game_mode == SCREEN_SAVER) {
						OpenMainMenu(&scene, &overlays);
					}
					else if (game_mode == PAUSED) {
						const Overlays::Texts selected = overlays.GetOption(Overlays::CONTINUE, Overlays::MOUSE);
						if (selected == Overlays::CONTINUE) {
							game_mode = PLAYING;
							scene.GetCurMusic().setVolume(GetVol());
							scene.SetExposure(1.0f);
							LockMouse(window);
						}
						else if (selected == Overlays::RESTART) {
							game_mode = PLAYING;
							scene.ResetLevel();
							scene.GetCurMusic().setVolume(GetVol());
							scene.SetExposure(1.0f);
							LockMouse(window);
						}
						else if (selected == Overlays::QUIT) {
							if (scene.IsSinglePlay()) 
							{
								OpenLevelMenu(&scene, &overlays);
							}
							else 
							{
								OpenMainMenu(&scene, &overlays);
							}
							scene.SetMode(Scene::INTRO);
							scene.StopAllMusic();
							menu_music.setVolume(GetVol());
							//menu_music.play();
						}
						else if (selected == Overlays::MUSIC) {
							game_settings.mute = !game_settings.mute;
							for (int i = 0; i < num_level_music; ++i) {
								level_music[i].setVolume(GetVol());
							}
						}
						else if (selected == Overlays::MOUSE) {
							game_settings.mouse_sensitivity = (game_settings.mouse_sensitivity + 1) % 3;
						}
					}
					if (game_mode == LEVEL_EDITOR)
					{
						if (scene.cur_ed_mode == Scene::EditorMode::PLACE_MARBLE
							|| scene.cur_ed_mode == Scene::EditorMode::PLACE_FLAG)
						{
							scene.cur_ed_mode = Scene::EditorMode::DEFAULT;
						}
					}
				}
				else if (event.mouseButton.button == sf::Mouse::Right) {
					io_state.mouse[2] = true; 
					io_state.mouse_press[2] = true;
					if (game_mode == PLAYING) {
						scene.ResetLevel();
					}
				}
			}
			else if (event.type == sf::Event::MouseButtonReleased) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					io_state.mouse[0] = false;
					mouse_pos = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
					mouse_clicked = false;
				}
				else if (event.mouseButton.button == sf::Mouse::Right) {
					io_state.mouse[2] = false;
				}
			}
			else if (event.type == sf::Event::MouseMoved) {
				mouse_pos = sf::Vector2i(event.mouseMove.x, event.mouseMove.y);
				io_state.mouse_pos = sf::Vector2f(mouse_pos.x, mouse_pos.y);
				if (scene.cur_ed_mode == Scene::EditorMode::PLACE_MARBLE)
				{
					Eigen::Vector3f marble_pos = scene.MouseRayCast(mouse_pos.x, mouse_pos.y, scene.level_copy.marble_rad);
					scene.level_copy.start_pos = marble_pos;
				}
				else if (scene.cur_ed_mode == Scene::EditorMode::PLACE_FLAG)
				{
					Eigen::Vector3f flag_pos = scene.MouseRayCast(mouse_pos.x, mouse_pos.y);
					scene.level_copy.end_pos = flag_pos;
				}
			}
			else if (event.type == sf::Event::MouseWheelScrolled) {
				mouse_wheel += event.mouseWheelScroll.delta;
				io_state.wheel = mouse_wheel;
			}
		}
	}
    //Check if the game was beat
    if (scene.GetMode() == Scene::FINAL && game_mode != CREDITS) {
      game_mode = CREDITS;
      scene.StopAllMusic();
      scene.SetExposure(0.5f);
      credits_music.play();
    } else if (scene.GetMode() == Scene::MIDPOINT && game_mode != MIDPOINT) {
      game_mode = MIDPOINT;
      scene.StopAllMusic();
      scene.SetExposure(0.5f);
      credits_music.play();
    }

    //Main game update
    if (game_mode == MAIN_MENU) {
      scene.UpdateCamera();
      overlays.UpdateMenu((float)mouse_pos.x, (float)mouse_pos.y);
    } else if (game_mode == CONTROLS) {
      scene.UpdateCamera();
      overlays.UpdateControls((float)mouse_pos.x, (float)mouse_pos.y);
    } else if (game_mode == LEVELS) {
      scene.UpdateCamera();
    } else if (game_mode == SCREEN_SAVER) {
      scene.UpdateCamera();
    } else if (game_mode == PLAYING || game_mode == CREDITS || game_mode == MIDPOINT || game_mode == LEVEL_EDITOR) {
      //Collect keyboard input
      const float force_lr =
        (all_keys[sf::Keyboard::Left] || all_keys[sf::Keyboard::A] ? -1.0f : 0.0f) +
        (all_keys[sf::Keyboard::Right] || all_keys[sf::Keyboard::D] ? 1.0f : 0.0f);
      const float force_ud =
        (all_keys[sf::Keyboard::Down] || all_keys[sf::Keyboard::S] ? -1.0f : 0.0f) +
        (all_keys[sf::Keyboard::Up] || all_keys[sf::Keyboard::W] ? 1.0f : 0.0f);
	  const float jump =
		  (all_keys[sf::Keyboard::Space] || all_keys[sf::Keyboard::S] ? true : false);

	  //Apply forces to marble and camera
	  scene.UpdateMarble(force_lr, force_ud, jump);


	  scene.free_camera_speed *= 1 + mouse_wheel * 0.05;
      //Collect mouse input
	  if (overlays.TWBAR_ENABLED)
	  {
		  
		  sf::Vector2i mouse_delta = sf::Vector2i(0, 0);
		  window.setMouseCursorVisible(true);
		  if (mouse_clicked)
		  {
			  mouse_delta = mouse_pos - mouse_prev_pos;
		  }
		 
		  float ms = mouse_sensitivity;
		  if (game_settings.mouse_sensitivity == 1) {
			  ms *= 0.5f;
		  }
		  else if (game_settings.mouse_sensitivity == 2) {
			  ms *= 0.25f;
		  }
		  const float cam_lr = float(-mouse_delta.x) * ms;
		  const float cam_ud = float(-mouse_delta.y) * ms;
		  const float cam_z = mouse_wheel * wheel_sensitivity;

		  scene.UpdateCamera(cam_lr, cam_ud, cam_z, mouse_clicked);
	  }
	  else
	  {
		  window.setMouseCursorVisible(false);
		  const sf::Vector2i mouse_delta = mouse_pos - screen_center;
		  sf::Mouse::setPosition(screen_center, window);
		  float ms = mouse_sensitivity;
		  if (game_settings.mouse_sensitivity == 1) {
			  ms *= 0.5f;
		  }
		  else if (game_settings.mouse_sensitivity == 2) {
			  ms *= 0.25f;
		  }
		  const float cam_lr = float(-mouse_delta.x) * ms;
		  const float cam_ud = float(-mouse_delta.y) * ms;
		  const float cam_z = mouse_wheel * wheel_sensitivity;

		  scene.UpdateCamera(cam_lr, cam_ud, cam_z, mouse_clicked);
	  }

 
    } else if (game_mode == PAUSED) {
      overlays.UpdatePaused((float)mouse_pos.x, (float)mouse_pos.y);
    }

    bool skip_frame = false;
    if (lag_ms >= 1000.0f / target_fps) {
      //If there is too much lag, just do another frame of physics and skip the draw
      lag_ms -= 1000.0f / target_fps;
      skip_frame = true;
    } else {
	  window.setVerticalSyncEnabled(VSYNC);
      //Update the shader values
      scene.Write(shader);
	  scene.WriteRenderer(rend);
	  shader.setUniform("render_texture", main_txt);
	  rend.SetOutputTexture(main_txt);

      //Setup full-screen shader
      sf::RenderStates states = sf::RenderStates::Default;
      states.shader = &shader;

      //Draw the fractal
      if (fullscreen) {
	    window.setActive(false);
		renderTexture.setActive(true);
        //Draw to the render texture
		rend.Render();
        renderTexture.draw(rect, states);
        renderTexture.display();

        //Draw render texture to main window
        sf::Sprite sprite(renderTexture.getTexture());
        sprite.setScale(float(screen_size.width) / float(resolution->width),
                        float(screen_size.height) / float(resolution->height));
        window.draw(sprite);
		renderTexture.setActive(false);
		window.setActive(true);
      } else {
	    window.setActive(true);
        //Draw directly to the main window
        window.draw(rect, states);
      }
    }

    //Draw text overlays to the window
    if (game_mode == MAIN_MENU) {
      overlays.DrawMenu(window);
    } else if (game_mode == CONTROLS) {
      overlays.DrawControls(window);
    } else if (game_mode == LEVELS) {
      overlays.DrawLevels(window);
    } else if (game_mode == PLAYING) {
      if (scene.GetMode() == Scene::ORBIT && scene.GetMarble().x() < 998.0f) {
        overlays.DrawLevelDesc(window, scene.level_copy.txt);
      } else if (scene.GetMode() == Scene::MARBLE && !scene.IsFreeCamera()) {
        overlays.DrawArrow(window, scene.GetGoalDirection());
      }
      if (!scene.HasCheats() || scene.GetCountdownTime() < 4 * 60) {
        overlays.DrawTimer(window, scene.GetCountdownTime(), scene.IsHighScore());
      }
      if (!scene.HasCheats() && scene.IsFullRun() && !scene.IsFreeCamera()) {
        overlays.DrawSumTime(window, scene.GetSumTime());
      }
      if (scene.HasCheats() && !scene.IsFreeCamera()) {
        overlays.DrawCheatsEnabled(window);
      }
      if (show_cheats) {
        overlays.DrawCheats(window);
      }
    } else if (game_mode == PAUSED) {
      overlays.DrawPaused(window);
      if (scene.HasCheats()) {
        overlays.DrawCheatsEnabled(window);
      }
    } else if (game_mode == CREDITS) {
      overlays.DrawCredits(window, scene.IsFullRun(), scene.GetSumTime());
    } else if (game_mode == MIDPOINT) {
      overlays.DrawMidPoint(window, scene.IsFullRun(), scene.GetSumTime());
    }
    if (!scene.IsFreeCamera() || game_mode == LEVEL_EDITOR) {
      overlays.DrawFPS(window, int(smooth_fps + 0.5f));
    }


	//new interface render stuff
	io_state.dt = prev_s;
	io_state.time += io_state.dt;
	UpdateAllObjects(&window, io_state);
	window.setView(default_window_view);

    if (!skip_frame) {
      //Finally display to the screen
	 
	  if(overlays.TWBAR_ENABLED)
		scene.Synchronize();
	  overlays.DrawAntTweakBar();
	  
      window.display();

      //If V-Sync is running higher than desired fps, slow down!
      const float s = clock.restart().asSeconds();
	  prev_s = s;
      if (s > 0.0f) {
        smooth_fps = smooth_fps*0.9f + std::min(1.0f / s, target_fps)*0.1f;
      }
      const float time_diff_ms = 1000.0f * (1.0f / target_fps - s);
      if (time_diff_ms > 0) {
        sf::sleep(sf::seconds(time_diff_ms / 1000.0f));
        lag_ms = std::max(lag_ms - time_diff_ms, 0.0f);
      } else if (time_diff_ms < 0) {
        lag_ms += std::max(-time_diff_ms, 0.0f);
      }
    }
  }

 // RemoveAllObjects();
  //UpdateAllObjects(&window, io_state);
  //Stop all music
  menu_music.stop();
  scene.StopAllMusic();
  credits_music.stop();
  scene.levels.SaveScoresToFile();
  game_settings.Save(settings_file);
  TwTerminate();

#ifdef _DEBUG
  system("pause");
#endif
  return 0;
}
