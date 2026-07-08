#pragma once

#include "Box.h"
#include "Camera.h"

#include <vector>
#include <string>
#include <unordered_map>

namespace Engine
{
	enum class ClassType
	{
		Box,
		Camera,
	};

	class ObjectManager
	{
	public:
		

	private:
		struct ObjectList
		{
			ClassType type;
			size_t index;
		};
		//Scene=> Type=> Name=> Index//
		std::vector<std::vector<std::unordered_map<std::string, int>>> m_SceneInComponentMap;
		std::unordered_map<std::string, ObjectList> m_AllMap;
	};
}

/*
サンプルコード

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

// 1. コンポーネントの型定義
// 末尾に「Count」を入れることで、自動的に型の総数を取得できるようにします
enum class ComponentType {
    Status,     // 0
    Transform,  // 1
    Physics,    // 2
    Count       // 3 (型の総数)
};

using PropertyMap = std::unordered_map<std::string, int>;

// 名前から「どのプールの」「何番目」を指すか
struct ObjectLocation {
    ComponentType type;
    size_t index;
};

class GameEngineRegistry {
private:
    // ★【改善】型ごとの vector をさらに vector でまとめた2次元構造
    // 構造: m_component_pools[型][オブジェクトのインデックス]
    std::vector<std::vector<PropertyMap>> m_component_pools;

    // 名前 ➔ 場所の全知辞書
    std::unordered_map<std::string, ObjectLocation> m_global_names;

public:
    GameEngineRegistry() {
        // コンポーネントの型の数（Count）だけ、内部の vector をあらかじめ準備する
        m_component_pools.resize(static_cast<size_t>(ComponentType::Count));
    }

    // オブジェクト（コンポーネント）の登録
    void RegisterComponent(const std::string& name, ComponentType type) {
        size_t type_idx = static_cast<size_t>(type);

        // 指定された型のプールに新しい実体を追加
        m_component_pools[type_idx].push_back(PropertyMap());
        size_t target_index = m_component_pools[type_idx].size() - 1;

        // 名前と場所をグローバルマップに登録
        m_global_names[name] = { type, target_index };
    }

    // ★【改善】if文が消えてスッキリした一発検索
    PropertyMap& GetComponentByName(const std::string& name) {
        const ObjectLocation& loc = m_global_names.at(name);

        size_t type_idx = static_cast<size_t>(loc.type);

        // 2次元 vector から直接参照を返す（if文の分岐が不要になり高速化）
        return m_component_pools[type_idx][loc.index];
    }
};

*/