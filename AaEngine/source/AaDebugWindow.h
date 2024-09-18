#pragma once

class AaRenderSystem;
class AaSceneManager;

class DebugWindow
{
public:

	void init(AaSceneManager* scene, AaRenderSystem* renderer);
	void deinit();

	void draw();

	AaSceneManager* scene;
};