#include "RoguelikeBattleProcessTask.h"

#include "Logger.hpp"
#include "Resource.h"
#include "Controller.h"
#include "AsstBattleDef.h"

#include "OcrImageAnalyzer.h"
#include "BattleImageAnalyzer.h"

bool asst::RoguelikeBattleProcessTask::_run()
{
    bool ret = get_stage_info();

    // 没有关卡信息剩下的都是扯淡。寄了，直接报错
    if (!ret) {
        battle_speedup();
        return false;
    }

    while (!analyze_opers_preview()) {
        std::this_thread::yield();
    }
    battle_speedup();

    for (const auto& action : m_copilot_actions) {
        do_action(action);
    }

    return true;
}

bool asst::RoguelikeBattleProcessTask::get_stage_info()
{
    LogTraceFunction;

    const auto& tile = Resrc.tile();
    bool calced = false;

    if (m_stage_name.empty()) {
        const auto stage_name_task_ptr = Task.get("BattleStageName");
        sleep(stage_name_task_ptr->pre_delay);

        constexpr int StageNameRetryTimes = 50;
        for (int i = 0; i != StageNameRetryTimes; ++i) {
            cv::Mat image = m_ctrler->get_image();
            OcrImageAnalyzer name_analyzer(image);

            name_analyzer.set_task_info(stage_name_task_ptr);
            if (!name_analyzer.analyze()) {
                continue;
            }

            for (const auto& tr : name_analyzer.get_result()) {
                auto side_info = tile.calc(tr.text, true);
                if (side_info.empty()) {
                    continue;
                }
                m_side_tile_info = std::move(side_info);
                m_normal_tile_info = tile.calc(tr.text, false);
                m_stage_name = tr.text;
                calced = true;
                break;
            }
            if (calced) {
                break;
            }
            // 有些性能非常好的电脑，加载画面很快；但如果使用了不兼容 gzip 的方式截图的模拟器，截图反而非常慢
            // 这种时候一共可供识别的也没几帧，还要考虑识别错的情况。所以这里不能 sleep
            std::this_thread::yield();
        }
    }
    else {
        m_side_tile_info = tile.calc(m_stage_name, true);
        m_normal_tile_info = tile.calc(m_stage_name, false);
        calced = true;
    }

    if (calced) {
#ifdef ASST_DEBUG
        auto normal_tiles = tile.calc(m_stage_name, false);
        cv::Mat draw = m_ctrler->get_image();
        for (const auto& [point, info] : normal_tiles) {
            using TileKey = TilePack::TileKey;
            static const std::unordered_map<TileKey, std::string> TileKeyMapping = {
                { TileKey::Invalid, "invalid" },
                { TileKey::Forbidden, "forbidden" },
                { TileKey::Wall, "wall" },
                { TileKey::Road, "road" },
                { TileKey::Home, "end" },
                { TileKey::EnemyHome, "start" },
                { TileKey::Floor, "floor" },
                { TileKey::Hole, "hole" },
                { TileKey::Telin, "telin" },
                { TileKey::Telout, "telout" }
            };

            cv::putText(draw, TileKeyMapping.at(info.key), cv::Point(info.pos.x, info.pos.y), 1, 1, cv::Scalar(0, 0, 255));
        }
#endif

        auto cb_info = basic_info_with_what("StageInfo");
        auto& details = cb_info["details"];
        details["name"] = m_stage_name;
        details["size"] = m_side_tile_info.size();
        callback(AsstMsg::SubTaskExtraInfo, cb_info);
    }
    else {
        callback(AsstMsg::SubTaskExtraInfo, basic_info_with_what("StageInfoError"));
    }

    if (Resrc.roguelike().contains_actions(m_stage_name)) {
        m_copilot_actions = Resrc.roguelike().get_actions(m_stage_name);
    }

    return calced;
}

bool asst::RoguelikeBattleProcessTask::wait_condition(const BattleAction& action)
{
    wait_kills(action.kills);

    auto& cor_action = dynamic_cast<const RoguelikeBattleAction&>(action);

    using OperPair = decltype(m_cur_opers_info)::value_type;

    // waiting_cost：
    // 如果有 roles 中有靠前的职业，但费用不够，是否等待。可选，默认 false
    // 为 true 时，会使用 roles 中，当前有的、最靠前的职业（一直等他费用够）
    // 为 false 时，会使用 roles 中，当前有的、费用够的中最靠前的职业

    if (cor_action.waiting_cost) {
        // 查找当前有的干员中，在 cor_action.roles 里排在最前面的职业
        auto role_iter = std::find_first_of(cor_action.roles.cbegin(), cor_action.roles.cend(),
            m_cur_opers_info.cbegin(), m_cur_opers_info.cend(),
            [](const BattleRole& role, const OperPair& pair) -> bool {
                return pair.second.role == role;
            });
        if (role_iter == cor_action.roles.cend()) {
            return false;
        }
        BattleRole waiting_role = *role_iter;

        // 等待该职业的干员费用够
        while (std::find_if(m_cur_opers_info.cbegin(), m_cur_opers_info.cend(),
            [&](const OperPair& pair) -> bool {
                return pair.second.available && pair.second.role == waiting_role;
            }) == m_cur_opers_info.cend()) {
            std::this_thread::yield();
            update_opers_info(m_ctrler->get_image());
        }
    }
    else {
        // 查找当前的可用干员中，在 cor_action.roles 里排在最前面的职业
        while (std::find_first_of(cor_action.roles.cbegin(), cor_action.roles.cend(),
            m_cur_opers_info.cbegin(), m_cur_opers_info.cend(),
            [](const BattleRole& role, const OperPair& pair) -> bool {
                // 任意职业费用够即可
                return pair.second.available && pair.second.role == role;
            }) == cor_action.roles.cend()) {
            std::this_thread::yield();
            update_opers_info(m_ctrler->get_image());
        }
    }

    return true;
}
