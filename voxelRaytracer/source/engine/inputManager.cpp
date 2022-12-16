#include "engine/inputManager.h"

#include <unordered_map>

static std::unordered_map<Keys, KeyStruct*> keyMap;

static float mouseDeltaX;
static float mouseDeltaY;

static float deltaMouseScroll;

InputManager::InputManager()
{

}

InputManager::~InputManager()
{

}

void InputManager::init()
{
	for (int i = 0; i != static_cast<int>(Keys::Last); i++)
	{
		KeyStruct* myKey = new KeyStruct();
		keyMap.insert({ static_cast<Keys>(i), myKey });
	}
}

void InputManager::shutdown()
{
	for (int i = 0; i != static_cast<int>(Keys::Last); i++)
	{
		KeyStruct* myKey = keyMap.find<Keys>(static_cast<Keys>(i))->second;
		delete myKey;
	}
}

KeyStruct* InputManager::getKey(Keys aKey)
{
	return keyMap.find(aKey)->second;
}

void InputManager::getMouseDeltaPosition(float* aX, float* aY)
{
	*aX = mouseDeltaX;
	*aY = mouseDeltaY;
}

float InputManager::getMouseScroll()
{
	return deltaMouseScroll;
}

void InputManager::InputDispatcher::update()
{
	for (int i = 0; i != static_cast<int>(Keys::Last); i++)
	{
		KeyStruct* myKey = keyMap.find<Keys>(static_cast<Keys>(i))->second;
		myKey->pressed = false;
		myKey->released = false;
	}

	mouseDeltaX = 0;
	mouseDeltaY = 0;

	deltaMouseScroll = 0;
}

void InputManager::InputDispatcher::updateKey(uint8_t aKeyNum, bool aPressed)
{
	Keys myKey;

	//find corrosponding key
	if (aKeyNum >= 65 && aKeyNum <= 90)
	{
		//letter
		myKey = static_cast<Keys>(aKeyNum - 65);
	}
	else if (aKeyNum >= 48 && aKeyNum <= 57)
	{
		// number
		myKey = static_cast<Keys>((aKeyNum - 48) + 26);
	}
	else if (aKeyNum == 32)
	{
		//space
		myKey = Keys::Space;
	}
	else return;

	// update key struct
	KeyStruct* myKeyStruct = keyMap.find<Keys>(myKey)->second;

	myKeyStruct->pressed = (aPressed && !myKeyStruct->heldDown);
	myKeyStruct->released = !aPressed;

	myKeyStruct->heldDown = aPressed;
}

void InputManager::InputDispatcher::updateMouse(float aDeltaX, float aDeltaY)
{
	mouseDeltaX = aDeltaX;
	mouseDeltaY = aDeltaY;
}

void InputManager::InputDispatcher::updateScroll(float aDeltaScroll)
{
	deltaMouseScroll = aDeltaScroll;
}
