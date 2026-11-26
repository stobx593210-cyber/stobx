#include "CodeGen.h"
#include "CorrectionEngine.h"
#include "DiagnosticUI.h"
#include "Lexer.h"
#include "Parser.h"
#include "SP.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

using namespace stobx;

namespace {

constexpr std::uintmax_t kMaxSourceBytes = 64ULL * 1024ULL * 1024ULL;

struct CliOptions {
    std::string inputPath;
    bool skipCorrectionPrompt = false;
    bool runAfterBuild = false;
};

bool parseCliArgs(int argc, char* argv[], CliOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-fix") {
            opts.skipCorrectionPrompt = true;
        } else if (arg == "--run") {
            opts.runAfterBuild = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return false;
        } else if (opts.inputPath.empty()) {
            opts.inputPath = arg;
        } else {
            std::cerr << "Error: Unexpected argument: " << arg << "\n";
            return false;
        }
    }

    if (opts.inputPath.empty()) {
        std::cerr << "Usage: ./stobx [--no-fix] [--run] <filename.stobx>\n";
        return false;
    }

    return true;
}

std::string shellEscape(const std::string& raw) {
#ifdef _WIN32
    std::string escaped = "\"";
    for (char ch : raw) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
#else
    std::string escaped = "'";
    for (char ch : raw) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped += ch;
        }
    }
    escaped += "'";
    return escaped;
#endif
}

std::string readFileContents(const std::string& path) {
    std::error_code szErr;
    const auto fileSize = std::filesystem::file_size(path, szErr);
    if (!szErr && fileSize > kMaxSourceBytes) {
        throw std::runtime_error("Source file exceeds maximum supported size: " + path);
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFileContents(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Could not write file: " + path);
    }
    out << content;
}

std::size_t clampDiagnosticOffset(
    std::size_t baseOffset,
    std::ptrdiff_t offsetAdjustment,
    std::size_t currentSize
) noexcept {
    const std::ptrdiff_t rawOffset =
        static_cast<std::ptrdiff_t>(baseOffset) + offsetAdjustment;
    if (rawOffset <= 0) {
        return 0u;
    }
    return std::min<std::size_t>(
        static_cast<std::size_t>(rawOffset),
        currentSize
    );
}

void printParserErrors(const Parser& parser) {
    for (const auto& error : parser.getErrors()) {
        std::cerr << error << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    CliOptions opts;
    if (!parseCliArgs(argc, argv, opts)) {
        return 1;
    }

    SP processor;
    if (!processor.process(opts.inputPath.c_str())) {
        std::cerr << "Error: Pre-processing failed for " << opts.inputPath << "\n";
        return 1;
    }

    try {
        std::string sourceCode = readFileContents(opts.inputPath);


        Lexer lexer(sourceCode);
        auto tokens = lexer.tokenize();

        Parser parser(tokens, sourceCode);
        auto ast = parser.parse();

        if (!parser.getErrors().empty()) {
            printParserErrors(parser);
        }

        bool anyFileModified = false;

        if (!parser.getDiagnostics().empty() && !opts.skipCorrectionPrompt) {
            DiagnosticUI ui;
            CorrectionEngine correctionEngine;

            std::string currentSourceCode = sourceCode;
            std::ptrdiff_t offsetAdjustment = 0;

            for (const auto& diagnostic : parser.getDiagnostics()) {
                ui.showDiagnostic(diagnostic);

                const std::string correction =
                    correctionEngine.suggestCorrection(diagnostic.id);
                ui.showCorrection(correction);

                Diagnostic adjustedDiagnostic = diagnostic;
                adjustedDiagnostic.offset = clampDiagnosticOffset(
                    diagnostic.offset,
                    offsetAdjustment,
                    currentSourceCode.size()
                );

                const std::string preview = correctionEngine.createPreview(
                    currentSourceCode,
                    adjustedDiagnostic
                );

                if (preview.empty()) {
                    std::cout << "Could not create correction preview.\n";
                    continue;
                }

                if (ui.askForCorrection()) {
                    writeFileContents(opts.inputPath, preview);
                    offsetAdjustment +=
                        static_cast<std::ptrdiff_t>(preview.size()) -
                        static_cast<std::ptrdiff_t>(currentSourceCode.size());
                    currentSourceCode = preview;
                    anyFileModified = true;
                } else {
                    std::cout << "\nCorrection cancelled.";
                    if (anyFileModified) {
                        std::cout << " (Earlier accepted changes were already saved to file.)";
                    }
                    std::cout << "\n";
                    return 1;
                }
            }

            if (anyFileModified) {
                sourceCode = currentSourceCode;
                Lexer updatedLexer(sourceCode);
                tokens = updatedLexer.tokenize();
                Parser updatedParser(tokens, sourceCode);

                ast = updatedParser.parse();
                parser = std::move(updatedParser);
            }
        }

        if (!parser.getErrors().empty()) {
            if (anyFileModified) {
                printParserErrors(parser);
            }
            return 1;
        }

        const std::filesystem::path inputFsPath(opts.inputPath);
        const std::filesystem::path stemName = inputFsPath.stem();
        if (stemName.empty()) {
            throw std::runtime_error("Invalid input file stem name for: " + opts.inputPath);
        }

        const std::filesystem::path outputDir =
            inputFsPath.parent_path().empty()
                ? std::filesystem::path(".")
                : inputFsPath.parent_path();
        const std::filesystem::path targetLlPath =
            outputDir / (stemName.string() + ".ll");
        const std::filesystem::path exePath = outputDir / stemName;

        std::error_code fsErr;
        if (std::filesystem::exists(exePath, fsErr) &&
            std::filesystem::is_directory(exePath, fsErr)) {
            throw std::runtime_error(
                "Output executable path collides with an existing directory: " +
                exePath.string()
            );
        }

        if (std::filesystem::exists(inputFsPath, fsErr) &&
            std::filesystem::exists(exePath, fsErr) &&
            std::filesystem::equivalent(inputFsPath, exePath, fsErr)) {
            throw std::runtime_error(
                "Refusing to overwrite source file with compiled binary: " +
                exePath.string()
            );
        }

        const std::filesystem::path defaultLlPath("output.ll");
        if (std::filesystem::exists(defaultLlPath, fsErr) && defaultLlPath != targetLlPath) {
            std::filesystem::remove(defaultLlPath, fsErr);
            std::filesystem::remove(targetLlPath, fsErr);
        }

        CodeGen codegen;
        codegen.generate(ast.get());
        codegen.printIR();

        if (std::filesystem::exists(defaultLlPath, fsErr) && defaultLlPath != targetLlPath) {
            std::filesystem::rename(defaultLlPath, targetLlPath, fsErr);
            if (fsErr) {
                throw std::runtime_error(
                    "Failed to move generated IR to: " + targetLlPath.string()
                );
            }
        }

        if (!std::filesystem::exists(targetLlPath, fsErr)) {
            throw std::runtime_error(
                "Generated LLVM IR file not found: " + targetLlPath.string()
            );
        }

        const std::string compileCmd =
            "clang " + shellEscape(targetLlPath.string()) +
            " -o " + shellEscape(exePath.string());
        const int compileResult = std::system(compileCmd.c_str());
        if (compileResult < 0) {
            throw std::runtime_error(
                "Failed to invoke shell/compiler for: " + targetLlPath.string()
            );
        }
        if (compileResult != 0) {
            throw std::runtime_error(
                "clang failed while compiling: " + targetLlPath.string()
            );
        }

        if (opts.runAfterBuild) {
            const std::string runCmd = shellEscape(exePath.string());
            const int runResult = std::system(runCmd.c_str());
            if (runResult < 0) {
                throw std::runtime_error(
                    "Failed to invoke shell to run executable: " + exePath.string()
                );
            }
            if (runResult != 0) {
                throw std::runtime_error(
                    "Compiled program exited with non-zero status: " + exePath.string()
                );
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
