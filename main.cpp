#include "chess4.hpp"

int main(int argc, char* argv[]){
    Game game;

    // Optional CLI flags
    for(int i=1;i<argc;i++){
        std::string arg=argv[i];
        if(arg=="--depth" && i+1<argc){
            game.engine.maxDepth = std::stoi(argv[++i]);
        } else if(arg=="--time" && i+1<argc){
            game.engine.timeLimit_ms = std::stoi(argv[++i]);
        } else if(arg=="--help"){
            std::cout << "Usage: chess4 [--depth N] [--time MS]\n"
                      << "  --depth N    AI search depth (default 5)\n"
                      << "  --time  MS   AI time limit in milliseconds (default 5000)\n";
            return 0;
        }
    }

    game.start();
    return 0;
}
