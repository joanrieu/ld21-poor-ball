all: game

game: game.cpp
	g++ -oPoorBall $^ -O2 -lsfml-graphics -lfmodex
