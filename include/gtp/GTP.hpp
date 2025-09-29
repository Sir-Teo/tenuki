#pragma once

#include "go/Board.hpp"
#include "search/Search.hpp"

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>

namespace gtp {

class Server {
public:
    Server(go::Board board,
           std::istream& in,
           std::ostream& out,
           search::SearchConfig search_config = {},
           std::shared_ptr<search::Evaluator> evaluator = nullptr);

    void run();

private:
    using HandlerResult = std::pair<bool, std::string>;
    using Handler = std::function<HandlerResult(const std::string& args)>;

    HandlerResult handle_protocol_version(const std::string& args);
    HandlerResult handle_name(const std::string& args);
    HandlerResult handle_version(const std::string& args);
    HandlerResult handle_boardsize(const std::string& args);
    HandlerResult handle_clear_board(const std::string& args);
    HandlerResult handle_komi(const std::string& args);
    HandlerResult handle_play(const std::string& args);
    HandlerResult handle_genmove(const std::string& args);
    HandlerResult handle_final_score(const std::string& args);
    HandlerResult handle_showboard(const std::string& args);
    HandlerResult handle_quit(const std::string& args);

    std::pair<bool, go::Move> parse_vertex(const std::string& vertex) const;
    std::string vertex_to_string(int vertex) const;
    std::string format_success(const std::string& id, const std::string& payload) const;
    std::string format_failure(const std::string& id, const std::string& message) const;

    void register_handlers();
    void reset_search();

    go::Board board_;
    std::istream& in_;
    std::ostream& out_;
    std::unordered_map<std::string, Handler> handlers_;
    std::unique_ptr<search::SearchAgent> search_agent_;
    search::SearchConfig search_config_{};
    int move_number_ = 0;
};

} // namespace gtp
