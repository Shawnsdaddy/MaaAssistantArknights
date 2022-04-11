#pragma once
#include "BattleProcessTask.h"

namespace asst
{
    class RoguelikeBattleProcessTask : public BattleProcessTask
    {
    public:
        using BattleProcessTask::BattleProcessTask;
        virtual ~RoguelikeBattleProcessTask() = default;

    protected:
        virtual bool _run() override;

        virtual bool get_stage_info() override;
        virtual bool wait_condition(const BattleAction& action) override;

        std::vector<RoguelikeBattleAction> m_copilot_actions;
    };
}
