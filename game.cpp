#include <list>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <fmodex/fmod.h>

#define WIDTH 800
#define HEIGHT 600
#define BALL_RADIUS 10.f
#define WALL_SIZE 40
#define WALLS 5
#define START_TIME_STEP 7
#define LEVEL_STEP_COUNT 3000

sf::Sprite ballSprite;
sf::Uint32 lives;

const sf::Vector2f wallSize(WALL_SIZE, WALL_SIZE);

enum Direction { NW, NE, SW, SE } ballDir = NE;
typedef std::list<sf::FloatRect> RectCont;
RectCont walls;

sf::Uint32 timeStep;

sf::Mutex gameMutex;
bool paused = true;

FMOD_SYSTEM* fmodSys;

bool music = true;

FMOD_SOUND
* titleSound,
* startSound,
* hitSound,
* badHitSound,
* clickSound,
* deathSound;

FMOD_CHANNEL
* titleChannel,
* startChannel,
* hitChannel,
* badHitChannel,
* clickChannel,
* deathChannel;

enum Sounds {
	NONE = 0,
	TITLE,
	START,
	HIT,
	BAD_HIT,
	CLICK,
	DEATH,
	SOUND_COUNT
};

#define SOUND(id, name, str) FMOD_System_GetChannel(fmodSys, id, & name##Channel); \
		FMOD_System_CreateSound(fmodSys, "assets/" str ".wav", FMOD_2D, 0, & name##Sound);
#define PLAY_PAUSE(name, pause) play(name##Sound, & name##Channel, pause)
#define PLAY(name) PLAY_PAUSE(name, false)

void play(FMOD_SOUND* sound, FMOD_CHANNEL** channel, bool pause) {

	if (music) {

		FMOD_System_PlaySound(fmodSys, FMOD_CHANNEL_REUSE, sound, pause, channel);

	}

}

bool bounce(RectCont& rects, const sf::Vector2f& dirVect) {

	bool hit = false;

	for (RectCont::const_iterator it = rects.begin(); it != rects.end(); ++it) {

		if (it->Contains(ballSprite.GetPosition()
				+ sf::Vector2f(dirVect.x * BALL_RADIUS, 0))) {

			hit = true;

			switch (ballDir) {
			case NW:
				ballDir = NE;
				break;
			case NE:
				ballDir = NW;
				break;
			case SW:
				ballDir = SE;
				break;
			case SE:
				ballDir = SW;
				break;
			}

		}

		if (it->Contains(ballSprite.GetPosition()
				+ sf::Vector2f(0, dirVect.y * BALL_RADIUS))) {

			hit = true;

			switch (ballDir) {
			case NW:
				ballDir = SW;
				break;
			case NE:
				ballDir = SE;
				break;
			case SW:
				ballDir = NW;
				break;
			case SE:
				ballDir = NE;
				break;
			}

		}

	}

	return hit;

}

void stepPhysics() {

	RectCont extWalls;
	{

		sf::FloatRect up(0, -WALL_SIZE, WIDTH, WALL_SIZE),
		down(0, HEIGHT, WIDTH, WALL_SIZE),
		left(-WALL_SIZE, 0, WALL_SIZE, HEIGHT),
		right(WIDTH, 0, WALL_SIZE, HEIGHT);

		extWalls.push_back(up);
		extWalls.push_back(down);
		extWalls.push_back(left);
		extWalls.push_back(right);

	}

	sf::Uint32 stepCount, sleepStepCount;
	bool timeToSleep = false;
	sf::Clock clock;

	while (true) {

		if (timeToSleep) {

			gameMutex.Lock();
			const sf::Uint32 ts = timeStep;
			gameMutex.Unlock();

			sf::Sleep(ts);

		}

		sf::Lock lock(gameMutex);

		if (not timeStep) {

			timeStep = START_TIME_STEP;
			stepCount = sleepStepCount = 0;
			clock.Reset();

			PLAY_PAUSE(start, true);

		}

		timeToSleep = not timeToSleep
				and clock.GetElapsedTime() / timeStep < stepCount + sleepStepCount;

		if (paused or not lives) {

			++sleepStepCount;
			continue;

		}

		if (++stepCount > LEVEL_STEP_COUNT) {

			if (not --timeStep) {

				timeStep = 1;

			}

			PLAY(start);

			stepCount = sleepStepCount = 0;
			clock.Reset();

		}

		const sf::Vector2f dirVect(
				(ballDir == NE or ballDir == SE) ? 1.f : -1.f,
				(ballDir == SW or ballDir == SE) ? 1.f : -1.f
		);

		ballSprite.SetPosition(ballSprite.GetPosition() + dirVect);

		if (bounce(extWalls, dirVect)) {

			if (--lives) {

				PLAY(badHit);

			} else {

				PLAY(death);

			}

		}

		if (bounce(walls, dirVect)) {

			PLAY(hit);

		}

	}

}

void limitWalls() {

	while (true) {

		sf::Sleep(1000);

		if (not paused and lives) {

			sf::Lock lock(gameMutex);

			if (walls.size() > WALLS / 2) {

				walls.pop_front();

			}

		}

	}

}

void reset() {

	sf::Lock lock(gameMutex);

	timeStep = 0;
	lives = 4;
	ballSprite.SetPosition(WIDTH / 2, HEIGHT / 2);
	walls.clear();

}

int main() {

	FMOD_System_Create(&fmodSys);
	FMOD_System_Init(fmodSys, SOUND_COUNT, FMOD_INIT_NORMAL, 0);

	SOUND(TITLE, title, "title");
	SOUND(START, start, "start");
	SOUND(HIT, hit, "hit");
	SOUND(BAD_HIT, badHit, "badHit");
	SOUND(CLICK, click, "click");
	SOUND(DEATH, death, "death");

	sf::RenderWindow win(
			sf::VideoMode(WIDTH, HEIGHT),
			"LD21 - Escape - Poor Ball by Fififox",
			sf::Style::Titlebar | sf::Style::Close,
			sf::ContextSettings(0, 0, 4)
	);

	win.EnableVerticalSync(true);
	win.EnableKeyRepeat(false);

	sf::Texture ballTexture, wallTexture, bgTexture, titleTexture;
	ballTexture.LoadFromFile("assets/ball.png");
	wallTexture.LoadFromFile("assets/wall.png");
	bgTexture.LoadFromFile("assets/bg.png");
	titleTexture.LoadFromFile("assets/title.png");

	sf::Sprite wallSprite, bgSprite, titleSprite;
	ballSprite.SetTexture(ballTexture);
	wallSprite.SetTexture(wallTexture);
	bgSprite.SetTexture(bgTexture);
	titleSprite.SetTexture(titleTexture);

	reset();

	sf::Thread physicsThread(&stepPhysics);
	physicsThread.Launch();

	sf::Thread limitThread(&limitWalls);
	limitThread.Launch();

	sf::Uint32 gameTime = 0;
	sf::Clock timer;

	while (win.IsOpened()) {

		sf::Event event;
		while (win.PollEvent(event)) {

			if (event.Type == sf::Event::Closed or
					(event.Type == sf::Event::KeyPressed
					and event.Key.Code == sf::Keyboard::Escape)) {

				if (paused) {

					win.Close();

				} else {

					paused = true;
					gameTime += timer.GetElapsedTime();

				}

			} else if (event.Type == sf::Event::KeyPressed) {

				if (event.Key.Code == sf::Keyboard::Back) {

					gameTime = 0;
					paused = false;
					timer.Reset();
					reset();
					continue;

				} else if (event.Key.Code == sf::Keyboard::M) {

					music = not music;
					continue;

				}

				if (not paused) {

					gameTime += timer.GetElapsedTime();

				} else {

					timer.Reset();

				}

				paused = not paused;

			} else if (event.Type == sf::Event::MouseButtonPressed) {

				if (paused and lives) {

					paused = false;
					timer.Reset();
					continue;

				} else if (paused or not lives) {

					continue;

				}

				sf::Lock lock(gameMutex);

				sf::FloatRect wall(
						sf::Vector2f(
								event.MouseButton.X,
								event.MouseButton.Y
						) - wallSize / 2.f,
						wallSize
				);

				bool intersect = false;
				for (RectCont::const_iterator it = walls.begin(); it != walls.end(); ++it) {

					if (it->Intersects(wall)) {

						intersect = true;
						break;

					}

				}

				if (intersect or wall.Contains(ballSprite.GetPosition())) {

					continue;

				}

				walls.push_back(wall);

				if (walls.size() > WALLS)
					walls.pop_front();

				PLAY(click);

			}

		}

		win.Draw(bgSprite);

		if (paused or not lives) {

			if (not lives and not paused) {

				paused = true;
				gameTime += timer.GetElapsedTime();

			} else if (lives) {

				void* userData;
				FMOD_Channel_GetUserData(titleChannel, &userData);

				if (not userData) {

					FMOD_Channel_SetUserData(titleChannel, titleChannel);
					PLAY(title);

				}

			}

			win.Draw(titleSprite);

			std::stringstream ss;

			if (not gameTime) {

				ss << "Click to start.\n"
						<< "Good Luck and Have Fun ! =D\n"
						<< "\tJoan `Fififox' Rieu";

			} else if (lives) {

				ss << "Game paused...";

			} else {

				ss << "Why did you let Ball escape ?!\n"
						<< "I was counting on you during those last "
						<< gameTime / 1000 << " seconds !";

			}

			sf::Text text(ss.str(), sf::Font::GetDefaultFont(), 15);
			text.SetColor(sf::Color(200, 200, 200));
			text.SetPosition(350, 480);
			win.Draw(text);

			win.Display();

			continue;

		}

		FMOD_Channel_SetUserData(titleChannel, 0);
		FMOD_Channel_SetPaused(startChannel, false);

		sf::Lock lock(gameMutex);

		for (RectCont::const_iterator it = walls.begin(); it != walls.end(); ++it) {

			wallSprite.SetPosition(it->Left, it->Top);
			win.Draw(wallSprite);

		}

		const sf::Vector2f ballPos = ballSprite.GetPosition();
		ballSprite.SetPosition(ballPos - sf::Vector2f(BALL_RADIUS, BALL_RADIUS));
		win.Draw(ballSprite);
		ballSprite.SetPosition(ballPos);

		std::stringstream ss;
		ss.precision(3);
		ss << "Time:\t" << (gameTime + timer.GetElapsedTime()) / 1000.f << "s\n";
		ss << "Level:\t" << (timeStep ? START_TIME_STEP + 1 - timeStep : 1) << "\n";
		ss << "Lives:\t" << lives - 1;

		sf::Text text(ss.str());
		text.SetStyle(sf::Text::Italic);
		text.SetCharacterSize(16);
		text.SetPosition(15, 15);
		win.Draw(text);

		win.Display();

	}

	FMOD_Channel_Stop(titleChannel);

	limitThread.Terminate();
	physicsThread.Terminate();

	return 0;

}
