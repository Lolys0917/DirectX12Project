//|| EntityTypes.h ||:::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  シーン、オブジェクト、コンポーネントの強いID型と種別を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
//||  2026_08_17  v1.10  Script Component種別と関数定義Comment形式を追加
//||  2026_07_13  v1.00  新規作成
//||

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace Engine
{
    class SceneID final
    {
    public:
        static constexpr std::uint32_t InvalidValue = 0; //無効なシーンID値

        //概要：無効なScene IDを作成する
        //引数：なし
        //戻り値：なし
        constexpr SceneID() noexcept = default;

        //概要：指定整数値からScene IDを作成する
        //引数：value=Scene ID値
        //戻り値：なし
        explicit constexpr SceneID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        //概要：Scene IDの整数値を取得する
        //引数：なし
        //戻り値：SceneManager内のID値
        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //概要：Scene IDが登録済みの有効値か判定する
        //引数：なし
        //戻り値：ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //概要：Scene ID値を三方向比較する
        //引数：other=比較対象Scene ID
        //戻り値：ID値の大小関係
        auto operator<=>(const SceneID& other) const = default;

    private:
        std::uint32_t Value = InvalidValue; //シーンID値
    };

    class ObjectID final
    {
    public:
        static constexpr std::uint32_t InvalidValue = 0; //無効なオブジェクトID値

        //概要：無効なObject IDを作成する
        //引数：なし
        //戻り値：なし
        constexpr ObjectID() noexcept = default;

        //概要：指定整数値からObject IDを作成する
        //引数：value=Object ID値
        //戻り値：なし
        explicit constexpr ObjectID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        //概要：Object IDの整数値を取得する
        //引数：なし
        //戻り値：Scene内のObject ID値
        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //概要：Object IDが登録済みの有効値か判定する
        //引数：なし
        //戻り値：ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //概要：Object ID値を三方向比較する
        //引数：other=比較対象Object ID
        //戻り値：ID値の大小関係
        auto operator<=>(const ObjectID& other) const = default;

    private:
        std::uint32_t Value = InvalidValue; //オブジェクトID値
    };

    class ComponentID final
    {
    public:
        static constexpr std::uint32_t InvalidValue = 0; //無効なコンポーネントID値

        //概要：無効なComponent IDを作成する
        //引数：なし
        //戻り値：なし
        constexpr ComponentID() noexcept = default;

        //概要：指定整数値からComponent IDを作成する
        //引数：value=Component ID値
        //戻り値：なし
        explicit constexpr ComponentID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        //概要：Component IDの整数値を取得する
        //引数：なし
        //戻り値：Scene内のComponent ID値
        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //概要：Component IDが登録済みの有効値か判定する
        //引数：なし
        //戻り値：ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //概要：Component ID値を三方向比較する
        //引数：other=比較対象Component ID
        //戻り値：ID値の大小関係
        auto operator<=>(const ComponentID& other) const = default;

    private:
        std::uint32_t Value = InvalidValue; //コンポーネントID値
    };

    enum class ObjectType : std::size_t
    {
        Object, //汎用オブジェクト
        Box, //直方体オブジェクト
        Sphere, //球オブジェクト
        Plane, //平面オブジェクト
        Cylinder, //円柱オブジェクト
        HalfSphere, //半球オブジェクト
        Capsule, //カプセルオブジェクト
		SkyBox, //スカイボックスオブジェクト
        Count //オブジェクト種別数
    };

    enum class ComponentType : std::size_t
    {
        Component, //汎用コンポーネント
        Mesh, //メッシュコンポーネント
        Polygon, //ポリゴンコンポーネント
        Model, //モデルコンポーネント
        Camera, //カメラコンポーネント
        Grid, //グリッドコンポーネント
        Collider, //汎用コライダー
        BoxCollider, //直方体コライダー
        SphereCollider, //球コライダー
        CapsuleCollider, //カプセルコライダー
        CylinderCollider, //円柱コライダー
        PlaneCollider, //平面コライダー
        Script, //オブジェクトへ差し込んで毎フレーム実行するスクリプト
        Count //コンポーネント種別数
    };
}

namespace std
{
    template<>
    struct hash<Engine::SceneID>
    {
        //シーンIDのハッシュ値を取得する
        //sceneID : 対象シーンID
        //戻り値 : ハッシュ値
        std::size_t operator()(const Engine::SceneID& sceneID) const noexcept
        {
            return std::hash<std::uint32_t>{}(sceneID.GetValue());
        }
    };

    template<>
    struct hash<Engine::ObjectID>
    {
        //オブジェクトIDのハッシュ値を取得する
        //objectID : 対象オブジェクトID
        //戻り値 : ハッシュ値
        std::size_t operator()(const Engine::ObjectID& objectID) const noexcept
        {
            return std::hash<std::uint32_t>{}(objectID.GetValue());
        }
    };

    template<>
    struct hash<Engine::ComponentID>
    {
        //コンポーネントIDのハッシュ値を取得する
        //componentID : 対象コンポーネントID
        //戻り値 : ハッシュ値
        std::size_t operator()(const Engine::ComponentID& componentID) const noexcept
        {
            return std::hash<std::uint32_t>{}(componentID.GetValue());
        }
    };
}
