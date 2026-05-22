// DaVinciCode.cpp : 局域网版达芬奇密码控制台游戏
// 编译说明: g++ -o davinci DaVinciCode.cpp -lws2_32 -std=c++11
// 或者在 Visual Studio 中直接创建项目并添加 GenSocket.h

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <cstring>
#include <cctype>
#include "GenSocket.h"

#ifdef _WIN32
    #include <windows.h>
    #define RESET_COLOR ""
#else
    #define RESET_COLOR "\033[0m"
#endif

// 颜色定义 (UTF-8 终端支持)
#define COLOR_WHITE "\033[97m"
#define COLOR_GRAY "\033[90m"
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[92m"
#define COLOR_RED "\033[91m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_CYAN "\033[96m"

// 游戏常量
const int MIN_NUMBER = 0;
const int MAX_NUMBER = 12;
const int TOTAL_TILES = 24; // 0-12 黑白各一张

// 牌的结构
struct Tile {
    int value;      // 数值 0-12
    bool isWhite;   // true=白牌, false=黑牌
    bool revealed;  // 是否已翻开
    
    std::string toString() const {
        return (isWhite ? "W" : "B") + std::to_string(value);
    }
    
    std::string toDisplayString() const {
        if (revealed) {
            if (isWhite) {
                return COLOR_WHITE + std::to_string(value) + COLOR_RESET;
            } else {
                return COLOR_GRAY + std::to_string(value) + COLOR_RESET;
            }
        } else {
            if (isWhite) {
                return COLOR_WHITE + "[?]" + COLOR_RESET;
            } else {
                return COLOR_GRAY + "[?]" + COLOR_RESET;
            }
        }
    }
};

// 玩家手牌
struct PlayerHand {
    std::vector<Tile> tiles;
    
    void sortTiles() {
        std::sort(tiles.begin(), tiles.end(), [](const Tile& a, const Tile& b) {
            if (a.value != b.value) return a.value < b.value;
            return a.isWhite > b.isWhite; // 同数值白牌在前
        });
    }
    
    void addTile(const Tile& tile) {
        tiles.push_back(tile);
        sortTiles();
    }
    
    bool revealTile(int index) {
        if (index >= 0 && index < (int)tiles.size()) {
            tiles[index].revealed = true;
            return true;
        }
        return false;
    }
    
    void display(bool showAll = false) const {
        std::cout << "  ";
        for (size_t i = 0; i < tiles.size(); ++i) {
            if (showAll || tiles[i].revealed) {
                std::cout << tiles[i].toDisplayString() << " ";
            } else {
                std::cout << (tiles[i].isWhite ? COLOR_WHITE : COLOR_GRAY) << "[?]" << COLOR_RESET << " ";
            }
        }
        std::cout << std::endl;
        std::cout << "  ";
        for (size_t i = 0; i < tiles.size(); ++i) {
            std::cout << "[" << i << "] ";
        }
        std::cout << std::endl;
    }
};

// 游戏状态枚举
enum class GameState {
    LOBBY,
    WAITING_START,
    PLAYING,
    GUESSING,
    GAME_OVER
};

// 网络消息类型
enum class MessageType {
    HELLO,
    START_GAME,
    GAME_STATE,
    MOVE,
    GUESS_RESULT,
    CHAT,
    GAME_OVER,
    ERROR
};

// 消息结构
struct GameMessage {
    MessageType type;
    std::string data;
    int playerId;
    int targetIndex;
    int guessValue;
    bool guessIsWhite;
    
    std::string serialize() const {
        std::string msg = std::to_string((int)type) + "|";
        msg += std::to_string(playerId) + "|";
        msg += std::to_string(targetIndex) + "|";
        msg += std::to_string(guessValue) + "|";
        msg += (guessIsWhite ? "1" : "0") + "|";
        msg += data;
        return msg;
    }
    
    static GameMessage deserialize(const std::string& str) {
        GameMessage msg;
        size_t pos = 0;
        size_t next;
        
        next = str.find('|', pos);
        msg.type = (MessageType)std::stoi(str.substr(pos, next - pos));
        pos = next + 1;
        
        next = str.find('|', pos);
        msg.playerId = std::stoi(str.substr(pos, next - pos));
        pos = next + 1;
        
        next = str.find('|', pos);
        msg.targetIndex = std::stoi(str.substr(pos, next - pos));
        pos = next + 1;
        
        next = str.find('|', pos);
        msg.guessValue = std::stoi(str.substr(pos, next - pos));
        pos = next + 1;
        
        next = str.find('|', pos);
        msg.guessIsWhite = (str.substr(pos, next - pos) == "1");
        pos = next + 1;
        
        msg.data = str.substr(pos);
        
        return msg;
    }
};

// 游戏主类
class DaVinciGame {
private:
    bool isServer;
    SOCKET gameSocket;
    int myPlayerId;
    GameState state;
    PlayerHand myHand;
    std::vector<PlayerHand> opponentHands;
    std::vector<std::string> playerNames;
    int currentPlayer;
    int totalPlayers;
    bool myTurn;
    std::string lastAction;
    
    std::vector<Tile> deck;
    
public:
    DaVinciGame() : isServer(false), gameSocket(INVALID_SOCKET), myPlayerId(-1), 
                    state(GameState::LOBBY), currentPlayer(0), totalPlayers(2), 
                    myTurn(false) {}
    
    bool startServer(const std::string& name) {
        if (!GenSocket_Init()) {
            std::cerr << "Failed to initialize Winsock" << std::endl;
            return false;
        }
        
        gameSocket = GenSocket_CreateServer(8888);
        if (gameSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create server socket" << std::endl;
            return false;
        }
        
        isServer = true;
        myPlayerId = 0;
        playerNames.push_back(name);
        state = GameState::LOBBY;
        
        std::cout << COLOR_GREEN << "Server started. Waiting for client..." << COLOR_RESET << std::endl;
        
        // 接受客户端连接
        GenClientConnection* client = GenSocket_Accept(gameSocket, "Client", "Remote player");
        if (!client) {
            std::cerr << "Failed to accept client" << std::endl;
            return false;
        }
        
        gameSocket = client->socket;
        totalPlayers = 2;
        playerNames.push_back("Client");
        opponentHands.resize(1);
        
        std::cout << COLOR_GREEN << "Client connected! Starting game..." << COLOR_RESET << std::endl;
        
        setupGame();
        return true;
    }
    
    bool connectToServer(const std::string& serverIP, const std::string& name) {
        if (!GenSocket_Init()) {
            std::cerr << "Failed to initialize Winsock" << std::endl;
            return false;
        }
        
        gameSocket = GenSocket_Connect(serverIP.c_str(), 8888);
        if (gameSocket == INVALID_SOCKET) {
            std::cerr << "Failed to connect to server" << std::endl;
            return false;
        }
        
        isServer = false;
        myPlayerId = 1;
        playerNames.push_back("Server");
        playerNames.push_back(name);
        state = GameState::LOBBY;
        
        std::cout << COLOR_GREEN << "Connected to server!" << COLOR_RESET << std::endl;
        
        totalPlayers = 2;
        opponentHands.resize(1);
        
        setupGame();
        return true;
    }
    
    void setupGame() {
        // 创建牌堆
        deck.clear();
        for (int i = MIN_NUMBER; i <= MAX_NUMBER; ++i) {
            deck.push_back({i, true, false});   // 白牌
            deck.push_back({i, false, false});  // 黑牌
        }
        
        // 洗牌
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(deck.begin(), deck.end(), g);
        
        // 发牌
        myHand.tiles.clear();
        opponentHands[0].tiles.clear();
        
        for (int i = 0; i < TOTAL_TILES; ++i) {
            if (i % 2 == 0) {
                myHand.addTile(deck[i]);
            } else {
                opponentHands[0].addTile(deck[i]);
            }
        }
        
        myHand.sortTiles();
        opponentHands[0].sortTiles();
        
        state = GameState::PLAYING;
        myTurn = (myPlayerId == 0); // 服务器先手
        
        std::cout << COLOR_CYAN << "\n=== Game Started! ===" << COLOR_RESET << std::endl;
        if (myTurn) {
            std::cout << COLOR_YELLOW << "Your turn!" << COLOR_RESET << std::endl;
        } else {
            std::cout << "Waiting for opponent..." << std::endl;
        }
    }
    
    void displayGame() {
        system("cls"); // Windows clear screen
        
        std::cout << COLOR_CYAN << "=== Da Vinci Code ===" << COLOR_RESET << std::endl;
        std::cout << "Players: " << playerNames[0] << " vs " << playerNames[1] << std::endl;
        std::cout << "Current turn: " << playerNames[currentPlayer] << std::endl;
        if (!lastAction.empty()) {
            std::cout << "Last action: " << lastAction << std::endl;
        }
        std::cout << std::endl;
        
        // 显示对手的手牌（未翻开的显示为[?]）
        std::cout << COLOR_RED << opponentHands[0].tiles.empty() ? "" : playerNames[1 - myPlayerId] << "'s hand:" << COLOR_RESET << std::endl;
        opponentHands[0].display(false);
        std::cout << std::endl;
        
        // 显示自己的手牌
        std::cout << COLOR_GREEN << "Your hand (" << playerNames[myPlayerId] << "):" << COLOR_RESET << std::endl;
        myHand.display(true);
        std::cout << std::endl;
        
        if (myTurn && state == GameState::PLAYING) {
            std::cout << COLOR_YELLOW << "Your options:" << COLOR_RESET << std::endl;
            std::cout << "1. Guess opponent's tile" << std::endl;
            std::cout << "2. Reveal your own tile (if applicable)" << std::endl;
            std::cout << "Enter command: ";
        }
    }
    
    void sendMove(int targetIndex, int guessValue, bool guessIsWhite) {
        GameMessage msg;
        msg.type = MessageType::MOVE;
        msg.playerId = myPlayerId;
        msg.targetIndex = targetIndex;
        msg.guessValue = guessValue;
        msg.guessIsWhite = guessIsWhite;
        msg.data = "";
        
        std::string serialized = msg.serialize();
        GenSocket_SendString(gameSocket, serialized);
    }
    
    void receiveMove() {
        char buffer[4096];
        int bytes = GenSocket_Receive(gameSocket, buffer, sizeof(buffer));
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            GameMessage msg = GameMessage::deserialize(std::string(buffer));
            
            if (msg.type == MessageType::MOVE) {
                processOpponentMove(msg.targetIndex, msg.guessValue, msg.guessIsWhite);
            } else if (msg.type == MessageType::GAME_OVER) {
                state = GameState::GAME_OVER;
                std::cout << COLOR_RED << "Game Over: " << msg.data << COLOR_RESET << std::endl;
            }
        }
    }
    
    void processOpponentMove(int targetIndex, int guessValue, bool guessIsWhite) {
        std::cout << "\nOpponent is guessing..." << std::endl;
        
        if (targetIndex < 0) {
            // 对手选择翻开自己的牌
            if (myHand.revealTile(-targetIndex - 1)) {
                lastAction = playerNames[1 - myPlayerId] + " revealed their own tile";
                std::cout << COLOR_YELLOW << "Opponent revealed one of their tiles!" << COLOR_RESET << std::endl;
            }
        } else {
            // 对手猜测我的牌
            if (targetIndex >= 0 && targetIndex < (int)myHand.tiles.size()) {
                Tile& tile = myHand.tiles[targetIndex];
                bool correct = (tile.value == guessValue && tile.isWhite == guessIsWhite);
                
                std::cout << "Opponent guesses: " << (guessIsWhite ? "White" : "Black") << " " << guessValue << std::endl;
                
                if (correct) {
                    std::cout << COLOR_GREEN << "Correct!" << COLOR_RESET << std::endl;
                    tile.revealed = true;
                    lastAction = playerNames[1 - myPlayerId] + " guessed correctly!";
                    
                    // 检查胜利条件
                    if (checkWinCondition()) {
                        sendGameOver(playerNames[1 - myPlayerId] + " wins!");
                        return;
                    }
                } else {
                    std::cout << COLOR_RED << "Wrong guess!" << COLOR_RESET << std::endl;
                    lastAction = playerNames[1 - myPlayerId] + " guessed incorrectly";
                }
            }
        }
        
        // 切换回合
        myTurn = !myTurn;
        currentPlayer = myPlayerId;
        
        std::cout << "\nPress Enter to continue...";
        std::cin.ignore();
    }
    
    bool checkWinCondition() {
        // 如果一方所有牌都被翻开，则另一方获胜
        bool allRevealed1 = true;
        for (const auto& tile : myHand.tiles) {
            if (!tile.revealed) {
                allRevealed1 = false;
                break;
            }
        }
        
        bool allRevealed2 = true;
        for (const auto& tile : opponentHands[0].tiles) {
            if (!tile.revealed) {
                allRevealed2 = false;
                break;
            }
        }
        
        if (allRevealed1) return true; // 对手赢
        if (allRevealed2) return true; // 我赢
        
        return false;
    }
    
    void sendGameOver(const std::string& message) {
        GameMessage msg;
        msg.type = MessageType::GAME_OVER;
        msg.data = message;
        msg.playerId = myPlayerId;
        msg.targetIndex = -1;
        msg.guessValue = -1;
        msg.guessIsWhite = false;
        
        std::string serialized = msg.serialize();
        GenSocket_SendString(gameSocket, serialized);
    }
    
    void playTurn() {
        displayGame();
        
        std::cout << "\nEnter your move (format: 'g <index> <value> <color>' or 'r <index>'): ";
        std::cout << "\n  g = guess opponent's tile (index 0-11, value 0-12, color w/b)" << std::endl;
        std::cout << "  r = reveal your own tile (index 0-11)" << std::endl;
        std::cout << "> ";
        
        std::string input;
        std::getline(std::cin, input);
        
        if (input.empty()) return;
        
        char cmd = input[0];
        if (cmd == 'g' || cmd == 'G') {
            // 猜测对手牌
            int index, value;
            char color;
            if (sscanf(input.c_str() + 1, "%d %d %c", &index, &value, &color) == 3) {
                if (index >= 0 && index < 12 && value >= 0 && value <= 12 && (color == 'w' || color == 'b')) {
                    bool isWhite = (color == 'w');
                    sendMove(index, value, isWhite);
                    myTurn = false;
                    std::cout << "Waiting for result..." << std::endl;
                } else {
                    std::cout << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
                }
            } else {
                std::cout << COLOR_RED << "Invalid format!" << COLOR_RESET << std::endl;
            }
        } else if (cmd == 'r' || cmd == 'R') {
            // 翻开自己的牌
            int index;
            if (sscanf(input.c_str() + 1, "%d", &index) == 1) {
                if (index >= 0 && index < (int)myHand.tiles.size()) {
                    // 发送负数索引表示翻开自己的牌
                    sendMove(-(index + 1), -1, false);
                    myTurn = false;
                    std::cout << "Waiting for confirmation..." << std::endl;
                } else {
                    std::cout << COLOR_RED << "Invalid index!" << COLOR_RESET << std::endl;
                }
            } else {
                std::cout << COLOR_RED << "Invalid format!" << COLOR_RESET << std::endl;
            }
        } else {
            std::cout << COLOR_RED << "Unknown command!" << COLOR_RESET << std::endl;
        }
    }
    
    void run() {
        while (state != GameState::GAME_OVER) {
            if (myTurn) {
                playTurn();
            } else {
                // 等待对手行动
                std::cout << "Waiting for opponent..." << std::endl;
                receiveMove();
                displayGame();
            }
            
            // 小延迟避免刷屏
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "\n" << COLOR_CYAN << "=== Final Results ===" << COLOR_RESET << std::endl;
        std::cout << "Your final hand:" << std::endl;
        myHand.display(true);
        std::cout << "\nOpponent's final hand:" << std::endl;
        opponentHands[0].display(true);
        
        GenSocket_Close(gameSocket);
        GenSocket_Cleanup();
    }
};

int main() {
    std::cout << COLOR_CYAN << "=== Da Vinci Code - Network Version ===" << COLOR_RESET << std::endl;
    std::cout << "1. Host Game (Server)" << std::endl;
    std::cout << "2. Join Game (Client)" << std::endl;
    std::cout << "Choose option: ";
    
    int choice;
    std::cin >> choice;
    std::cin.ignore();
    
    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);
    
    DaVinciGame game;
    
    if (choice == 1) {
        if (game.startServer(name)) {
            game.run();
        }
    } else if (choice == 2) {
        std::cout << "Enter server IP: ";
        std::string ip;
        std::getline(std::cin, ip);
        
        if (game.connectToServer(ip, name)) {
            game.run();
        }
    } else {
        std::cout << COLOR_RED << "Invalid choice!" << COLOR_RESET << std::endl;
    }
    
    return 0;
}
