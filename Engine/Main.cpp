import std;
import Application;

int main() {
    Application app;

    return app.run().value_or(1);
}