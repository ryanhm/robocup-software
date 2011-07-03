#include "YankChip.hpp"

#include <Utils.hpp>

using namespace std;
using namespace Geometry2d;

static const float Chip_Complete_Dist = 0.5;

static const float Min_Ball_Velocity = 0.2;

static const float Max_Yank_Dist = 0.2;

Gameplay::Behaviors::YankChip::YankChip(GameplayModule *gameplay):
    SingleRobotBehavior(gameplay), _capture(gameplay)
{
	restart();
	
	target = Point(0.0, Field_Length);

	_yank_travel_thresh = config()->createDouble("Yank/Ball Travel Threshold", 0.5);
	_max_aim_error = config()->createDouble("Yank/Ball Max Trajectory Error", 0.3);
}

void Gameplay::Behaviors::YankChip::restart()
{
	_state = State_Capture;
	_capture.restart();
	enable_yank = true;
	enable_aiming = true;
	backup_distance = 0.5;
	chip_speed = 255;
	dribble_speed = 127;  // use maximum dribbler speed by default
}

bool Gameplay::Behaviors::YankChip::run()
{
	if (!robot || !robot->visible)
	{
		return false;
	}
	
	// force sub-behavior to have its robot assigned
	_capture.robot = robot;

	// The direction we're facing
	const Point dir = Point::direction(robot->angle * DegreesToRadians);
	
	// State changes
	Segment fixedYankLine(robot->pos, _yankBallStart);
	if (_state == State_Capture)
	{
		if (robot->hasBall() && _capture.done() && enable_yank)
		{
			_yankBallStart = ball().pos;
			_yankRobotStart = robot->pos;
			_state = State_Yank;
		}
	} else if (_state == State_Yank)
	{
		// if ball has gotten away or didn't move with us, reset to capture
		if (!fixedYankLine.nearPoint(ball().pos, *_max_aim_error) ||
				(ball().vel.mag() < Min_Ball_Velocity && !ball().pos.nearPoint(robot->pos, Max_Yank_Dist)) || // if stationary and too far away
				(ball().vel.mag() > Min_Ball_Velocity && ball().vel.normalized().dot(-dir) >= cos(20 * DegreesToRadians))) { // if moving and in the wrong direction
			_state = State_Capture;
		}

		// move backwards until a fixed distance from opponent
		float err = (oppRobot) ? oppRobot->pos.distTo(ball().pos) : _yankBallStart.distTo(ball().pos);
		if (err > backup_distance)
		{
			_state = State_Chip;
		}
	} else if (_state == State_Chip)
	{
		// done after ball kicked
		if (_kicked && !ball().pos.nearPoint(robot->pos, Chip_Complete_Dist))
		{
			_state = State_Done;
		}

		// if ball got too far away, reset
		if (!_kicked && !ball().pos.nearPoint(robot->pos, Chip_Complete_Dist))
		{
			_state = State_Capture;
		}
	}
	
	// Driving
	if (_state == State_Capture)
	{
		robot->addText("Capturing");
		_capture.enable_pivot = enable_aiming;
		_capture.target = target;
		_capture.run();
	}  else if (_state == State_Yank)
	{
		robot->addText("Yanking");
		state()->drawLine(fixedYankLine, Qt::white);
		robot->avoidBall(-1.0);
		robot->worldVelocity(fixedYankLine.delta() * 5.0);
		robot->angularVelocity(0.0);
		robot->dribble(dribble_speed);
	}  else if (_state == State_Chip)
	{
		robot->addText("Chip");
		state()->drawLine(fixedYankLine, Qt::red);
		robot->avoidBall(-1.0);
		robot->chip(chip_speed);
		robot->dribble(dribble_speed);
		robot->worldVelocity(fixedYankLine.delta() * -5.0);
		robot->angularVelocity(0.0);
	} else {
		robot->addText("Done");
		return false;
	}
	
	return true;
}