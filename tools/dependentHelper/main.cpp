//
// Created by a1234 on 2025/12/28.
//
#include <filesystem>
#include <set>
#include <map>
#include <string>
#include <fstream>
#include <regex>
#include <iostream>
#include <memory>
#include <algorithm>

struct HeaderNode {
    std::filesystem::path path;
    std::set<std::shared_ptr<HeaderNode> > parents;
    std::set<std::shared_ptr<HeaderNode> > children;
};

std::set<std::string> getParents(std::filesystem::path path) {
    std::set<std::string> headers;
    std::ifstream file(path);
    std::string line;
    std::regex includeRegex(R"(^\s*#\s*include\s*["](.+)["])");
    auto parentPath = path.parent_path();
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, includeRegex)) {
            std::filesystem::path includePath = match[1].str();
            if (includePath.is_relative()) {
                headers.insert((parentPath / includePath).lexically_normal());
            } else {
                headers.insert(includePath);
            }
        }
    }
    return headers;
}

void CreateHeaderTree(std::map<std::string, std::shared_ptr<HeaderNode> > &headers,
                      std::filesystem::path _path,std::filesystem::path root) {
    std::filesystem::path path = _path.is_relative()? (root / _path).lexically_normal() : _path;
    auto parents = getParents(path);
    auto pathStr = path.string();

    // 如果已經處理過，直接返回
    if (pathStr == "/mnt/c/Users/a1234/CLionProjects/TypeSQLite/src/SQliteStruct/Column/Column.hpp") {
        std::cout<<"debug"<<std::endl;
    }
    if (headers.contains(pathStr)) {
        return;
    }

    auto currentNode = std::make_shared<HeaderNode>(HeaderNode{.path = path});
    headers.insert({pathStr, currentNode});

    for (const auto &parent: parents) {
        if (!headers.contains(parent)) {
            CreateHeaderTree(headers, parent,root);
        }
        headers[parent]->children.insert(currentNode);
        currentNode->parents.insert(headers[parent]);
    }
}

// 左旋樹輸出（豎向樹狀圖）
void printLeftRotatedTree(const std::set<std::shared_ptr<HeaderNode> > &tree,
                          const std::string &prefix,
                          bool isLast,
                          std::set<std::filesystem::path> &visited) {
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        const auto &node = *it;
        bool isLastNode = (std::next(it) == tree.end());

        // 檢查循環依賴
        if (visited.contains(node->path)) {
            std::cout << prefix;
            std::cout << (isLastNode ? "└── " : "├── ");
            std::cout << node->path.filename().string() << " [CIRCULAR]" << std::endl;
            continue;
        }

        visited.insert(node->path);

        // 輸出當前節點
        std::cout << prefix;
        std::cout << (isLastNode ? "└── " : "├── ");
        std::cout << node->path.filename().string() << std::endl;

        // 遞迴輸出父節點（依賴）
        if (!node->parents.empty()) {
            std::string newPrefix = prefix;
            if (isLastNode) {
                newPrefix += "    "; // 4 個空格
            } else {
                newPrefix += "│   "; // 豎線 + 3 個空格
            }
            printLeftRotatedTree(node->parents, newPrefix, false, visited);
        }

        visited.erase(node->path);
    }
}

// 公開接口（自動創建 visited）
void printLeftRotatedTree(const std::set<std::shared_ptr<HeaderNode> > &tree) {
    std::set<std::filesystem::path> visited;
    printLeftRotatedTree(tree, "", true, visited);
}

// 以 Graphviz DOT 格式輸出依賴圖
void printGraphvizDot(const std::map<std::string, std::shared_ptr<HeaderNode> > &headers,
                      const std::filesystem::path &rootPath,
                      const std::string &outputFile =
                              "/mnt/c/Users/a1234/CLionProjects/TypeSQLite/dependency_graph.dot") {
    std::ofstream dotFile(outputFile);
    if (!dotFile.is_open()) {
        std::cerr << "Error: Cannot create file: " << outputFile << std::endl;
        return;
    }

    dotFile << "digraph DependencyGraph {\n";
    dotFile << "    rankdir=LR;\n"; // 從左到右排列
    dotFile << "    node [shape=box, style=rounded, fontname=\"Arial\"];\n";
    dotFile << "    edge [color=\"#666666\"];\n\n";

    // 為每個節點添加標籤和樣式
    std::set<std::string> processedNodes;
    for (const auto &[pathStr, node]: headers) {
        std::string nodeName = node->path.filename().string();
        std::string nodeId = std::to_string(std::hash<std::string>{}(pathStr));

        // 根節點使用特殊顏色
        if (pathStr == rootPath.string()) {
            dotFile << "    \"" << nodeId << "\" [label=\"" << nodeName
                    << "\", fillcolor=\"#FFD700\", style=\"rounded,filled\"];\n";
        } else {
            dotFile << "    \"" << nodeId << "\" [label=\"" << nodeName
                    << "\", fillcolor=\"#E8E8E8\", style=\"rounded,filled\"];\n";
        }
        processedNodes.insert(pathStr);
    }

    dotFile << "\n";

    // 添加依賴關係邊
    std::set<std::pair<std::string, std::string> > processedEdges;
    for (const auto &[pathStr, node]: headers) {
        std::string fromId = std::to_string(std::hash<std::string>{}(pathStr));

        for (const auto &parent: node->parents) {
            std::string toId = std::to_string(std::hash<std::string>{}(parent->path.string()));

            // 避免重複邊
            auto edge = std::make_pair(fromId, toId);
            if (!processedEdges.contains(edge)) {
                dotFile << "    \"" << fromId << "\" -> \"" << toId << "\";\n";
                processedEdges.insert(edge);
            }
        }
    }

    dotFile << "}\n";
    dotFile.close();

    std::cout << "\n✅ Graphviz DOT file generated: " << outputFile << std::endl;

    // 自動執行 dot 命令生成圖片
    std::filesystem::path dotPath(outputFile);
    std::string pngOutput = dotPath.parent_path().string() + "/dependency_graph.png";
    std::string svgOutput = dotPath.parent_path().string() + "/dependency_graph.svg";

    std::cout << "\n🖼️  Generating PNG image..." << std::endl;
    std::string pngCmd = "dot -Tpng " + outputFile + " -o " + pngOutput;
    int pngResult = system(pngCmd.c_str());

    if (pngResult == 0) {
        std::cout << "✅ PNG generated: " << pngOutput << std::endl;
    } else {
        std::cerr << "❌ Failed to generate PNG. Is Graphviz installed?" << std::endl;
        std::cerr << "   Install with: sudo apt-get install graphviz" << std::endl;
    }

    std::cout << "\n🖼️  Generating SVG image..." << std::endl;
    std::string svgCmd = "dot -Tsvg " + outputFile + " -o " + svgOutput;
    int svgResult = system(svgCmd.c_str());

    if (svgResult == 0) {
        std::cout << "✅ SVG generated: " << svgOutput << std::endl;
    } else {
        std::cerr << "❌ Failed to generate SVG" << std::endl;
    }
}

// 以 ASCII 藝術圖形方式顯示（適合終端輸出）- 內部實現
void printAsciiGraphImpl(const std::set<std::shared_ptr<HeaderNode> > &tree,
                         const std::string &prefix,
                         bool isLast,
                         std::set<std::filesystem::path> &visited) {
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        const auto &node = *it;
        bool isLastNode = (std::next(it) == tree.end());

        // 檢查循環依賴
        if (visited.contains(node->path)) {
            std::cout << prefix;
            std::cout << (isLastNode ? "╚═══ " : "╠═══ ");
            std::cout << "⟲ " << node->path.filename().string() << " [CIRCULAR]" << std::endl;
            continue;
        }

        visited.insert(node->path);

        // 輸出當前節點
        std::cout << prefix;
        std::cout << (isLastNode ? "╚═══ " : "╠═══ ");

        // 添加圖標
        if (node->parents.empty()) {
            std::cout << "📄 "; // 葉節點
        } else {
            std::cout << "📦 "; // 有依賴的節點
        }
        std::cout << node->path.filename().string() << std::endl;

        // 遞迴輸出父節點（依賴）
        if (!node->parents.empty()) {
            std::string newPrefix = prefix;
            if (isLastNode) {
                newPrefix += "    ";
            } else {
                newPrefix += "║   ";
            }
            printAsciiGraphImpl(node->parents, newPrefix, false, visited);
        }

        visited.erase(node->path);
    }
}

// 公開接口
void printAsciiGraph(const std::set<std::shared_ptr<HeaderNode> > &tree) {
    std::set<std::filesystem::path> visited;
    printAsciiGraphImpl(tree, "", true, visited);
}

int main() {
    std::filesystem::path target = "/mnt/c/Users/a1234/CLionProjects/TypeSQLite/src/TypeSQlite.hpp";
    std::filesystem::path rootPath = target.parent_path();

    std::cout << "=== Direct includes ===" << std::endl;
    std::ranges::for_each(
        getParents(target),
        [](const auto &str) { std::cout << str << std::endl; }
    );

    std::cout << "\n=== Building dependency tree ===" << std::endl;
    std::map<std::string, std::shared_ptr<HeaderNode> > headers;
    CreateHeaderTree(headers, target,target.parent_path());

    // 找到根節點（target 文件）
    auto targetStr = target.string();

    if (!headers.contains(targetStr)) {
        std::cerr << "Error: Could not find target in headers" << std::endl;
        return 1;
    }

    std::cout << "\n=== Generating Graphviz DOT file ===" << std::endl;
    printGraphvizDot(headers, target, "/mnt/c/Users/a1234/CLionProjects/TypeSQLite/dependency_graph.dot");

    std::cout << "\n=== Statistics ===" << std::endl;
    std::cout << "Total headers: " << headers.size() << std::endl;

    return 0;
}
