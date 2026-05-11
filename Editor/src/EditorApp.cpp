import std;
import Engine.Core;

int main(int argc, char* argv[]) {
	Engine::Core::Application app;

	return app.run().value_or(1);
}