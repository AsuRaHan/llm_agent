#include "FinalAnswerTool.h"
#include "../Logger.h"

std::string FinalAnswerTool::getName() const {
    return "final_answer";
}

std::string FinalAnswerTool::getDescription() const {
    return "Используй этот инструмент, когда у тебя есть окончательный ответ на запрос пользователя. Передай свой полный и исчерпывающий ответ в аргументе 'answer'.";
}

nlohmann::json FinalAnswerTool::getParameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"answer", {
                {"type", "string"},
                {"description", "Полный и исчерпывающий ответ на запрос пользователя."}
            }}
        }},
        {"required", {"answer"}}
    };
}

std::string FinalAnswerTool::execute(const nlohmann::json& args, ContextIndexer* indexer) {
    (void)indexer; // Not used by this tool
    if (!args.contains("answer")) {
        SPDLOG_ERROR("[Tool:final_answer] Отсутствует обязательный аргумент 'answer'.");
        return "{\"error\": \"Отсутствует обязательный аргумент 'answer' для final_answer.\"}";
    }
    std::string final_answer_text = args["answer"].get<std::string>();
    SPDLOG_INFO("[Tool:final_answer] LLM предоставил финальный ответ через инструмент.");
    return final_answer_text; // This will be captured by QueryProcessor
}