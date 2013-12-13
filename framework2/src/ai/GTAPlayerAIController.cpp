#include "renderwure/ai/GTAPlayerAIController.hpp"
#include <renderwure/objects/GTACharacter.hpp>
#include <renderwure/objects/GTAVehicle.hpp>
#include <renderwure/engine/GTAEngine.hpp>
#include <glm/gtc/matrix_transform.hpp>

GTAPlayerAIController::GTAPlayerAIController(GTACharacter* character)
	: GTAAIController(character), lastRotation(glm::vec3(0.f, 0.f, 0.f)), running(false)
{
	
}

void GTAPlayerAIController::setRunning(bool run)
{
	running = run;
}

void GTAPlayerAIController::updateCameraDirection(const glm::quat& rot)
{
	cameraRotation = rot;
}

void GTAPlayerAIController::updateMovementDirection(const glm::vec3& dir)
{
	direction = dir;
}

void GTAPlayerAIController::exitVehicle()
{
	if(character->getCurrentVehicle()) {
		// Determine the seat location and teleport to outside the vehicle.
		auto vehicle = character->getCurrentVehicle();
		auto seatPos = vehicle->info.seats[character->getCurrentSeat()].offset;
		seatPos.x += 1.f * glm::sign(seatPos.x);
		glm::mat4 vehicleMatrix;
		vehicleMatrix = glm::translate(vehicleMatrix, vehicle->getPosition());
		vehicleMatrix = vehicleMatrix * glm::mat4_cast(vehicle->getRotation());
		glm::vec3 worldp(vehicleMatrix * glm::vec4(seatPos.x, seatPos.y, seatPos.z, 1.f));
		character->enterVehicle(nullptr, 0);
		character->setPosition(worldp);
	}
}

void GTAPlayerAIController::enterNearestVehicle()
{
	if(! character->getCurrentVehicle()) {
		auto world = character->engine;
		GTAVehicle* nearest = nullptr; float d = 10.f;
		for(auto it = world->vehicleInstances.begin(); it != world->vehicleInstances.end(); ++it) {
			float vd = glm::length( character->getPosition() - (*it)->getPosition());
			if( vd < d ) { 
				d = vd;
				nearest = *it;
			}
		}
		
		if( nearest ) {
			character->enterVehicle(nearest, 0);
		}
	}
}

void GTAPlayerAIController::update(float dt)
{
	if( glm::length(direction) > 0.001f ) {
		character->changeAction(running ? GTACharacter::Run : GTACharacter::Walk);
	}
	else {
		character->changeAction(GTACharacter::Idle);
	}

	if( character->getCurrentVehicle() ) {
		character->getCurrentVehicle()->setSteeringAngle(-direction.x * 3.131f);

		// TODO what is handbraking.
		character->getCurrentVehicle()->setThrottle(direction.y);
	}
	else if( glm::length(direction) > 0.001f ) {
		character->rotation = cameraRotation * glm::quat(glm::vec3(0.f, 0.f, -atan2(direction.x, direction.y)));
	}

}

glm::vec3 GTAPlayerAIController::getTargetPosition()
{
	return direction;
}
