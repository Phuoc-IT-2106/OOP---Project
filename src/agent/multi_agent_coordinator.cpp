#include "multi_agent_coordinator.h"

#include "message_queue.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <functional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace oop_agent::agent {
namespace {

bool isBlank(const std::string &value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

AgentRunResult failedRun(std::string message) {
    AgentRunResult result;
    result.error_message = std::move(message);
    return result;
}

void closeMailboxes(MessageQueue<AgentMessage> &inbox,
                    MessageQueue<AgentMessage> &peer_inbox) noexcept {
    try {
        inbox.close();
    } catch (...) {
    }
    try {
        peer_inbox.close();
    } catch (...) {
    }
}

void runWorker(AgentLoop &agent,
               const std::string &agent_id,
               const std::string &peer_id,
               const std::string &subtask,
               MessageQueue<AgentMessage> &inbox,
               MessageQueue<AgentMessage> &peer_inbox,
               AgentWorkerResult &worker_result) noexcept {
    try {
        worker_result.agent_id = agent_id;
        try {
            worker_result.run_result = agent.run(subtask);
        } catch (const std::exception &error) {
            worker_result.run_result =
                failedRun("agent thread failed: " + std::string(error.what()));
        } catch (...) {
            worker_result.run_result =
                failedRun("agent thread failed with an unknown exception");
        }

        AgentMessage outgoing;
        outgoing.sender_id = agent_id;
        outgoing.recipient_id = peer_id;
        outgoing.success = worker_result.run_result.success;
        outgoing.content = worker_result.run_result.success
                               ? worker_result.run_result.final_answer
                               : worker_result.run_result.error_message;

        if (!peer_inbox.push(std::move(outgoing))) {
            worker_result.communication_error = "peer mailbox was closed";
            return;
        }

        worker_result.peer_message = inbox.waitPop();
        if (!worker_result.peer_message.has_value()) {
            worker_result.communication_error =
                "own mailbox closed before peer response arrived";
        }
    } catch (const std::exception &error) {
        try {
            worker_result.communication_error =
                "message worker failed: " + std::string(error.what());
        } catch (...) {
        }
        closeMailboxes(inbox, peer_inbox);
    } catch (...) {
        try {
            worker_result.communication_error =
                "message worker failed with an unknown exception";
        } catch (...) {
        }
        closeMailboxes(inbox, peer_inbox);
    }
}

} // namespace

bool MultiAgentResult::success() const noexcept {
    return first.run_result.success && second.run_result.success &&
           first.peer_message.has_value() && second.peer_message.has_value() &&
           first.communication_error.empty() &&
           second.communication_error.empty();
}

MultiAgentCoordinator::MultiAgentCoordinator(AgentLoop &first_agent,
                                             AgentLoop &second_agent,
                                             MultiAgentConfig config)
    : first_agent_(first_agent),
      second_agent_(second_agent),
      config_(std::move(config)) {
    if (&first_agent_ == &second_agent_) {
        throw std::invalid_argument(
            "multi-agent coordinator requires two distinct AgentLoop instances");
    }
    if (isBlank(config_.first_agent_id) || isBlank(config_.second_agent_id)) {
        throw std::invalid_argument("agent ids must not be empty");
    }
    if (config_.first_agent_id == config_.second_agent_id) {
        throw std::invalid_argument("agent ids must be unique");
    }
}

MultiAgentResult MultiAgentCoordinator::run(const ParallelAgentTask &task) {
    if (isBlank(task.first_subtask) || isBlank(task.second_subtask)) {
        throw std::invalid_argument("both parallel subtasks must not be empty");
    }

    MessageQueue<AgentMessage> first_inbox;
    MessageQueue<AgentMessage> second_inbox;
    MultiAgentResult result;

    std::thread first_thread(
        runWorker, std::ref(first_agent_), std::cref(config_.first_agent_id),
        std::cref(config_.second_agent_id), std::cref(task.first_subtask),
        std::ref(first_inbox), std::ref(second_inbox), std::ref(result.first));

    std::thread second_thread;
    try {
        second_thread = std::thread(
            runWorker, std::ref(second_agent_),
            std::cref(config_.second_agent_id),
            std::cref(config_.first_agent_id),
            std::cref(task.second_subtask), std::ref(second_inbox),
            std::ref(first_inbox), std::ref(result.second));
    } catch (...) {
        first_inbox.close();
        second_inbox.close();
        first_thread.join();
        throw;
    }

    first_thread.join();
    second_thread.join();
    return result;
}

} // namespace oop_agent::agent
