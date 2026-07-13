//|| EntityTypes.h ||:::::::::::::::::::::::::::
//||
//||  概要 ::::::::::::::::::::::::::::::::::::
//||
//||  シーン、オブジェクト、コンポーネントの強いID型と種別を定義する
//||
//||  更新内容 ::::::::::::::::::::::::::::::::
//||
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

        //無効なシーンIDを作成する
        constexpr SceneID() noexcept = default;

        //指定値からシーンIDを作成する
        //value : シーンID値
        explicit constexpr SceneID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //シーンIDが登録済みの有効値か判定する
        //戻り値: ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //シーンID値を比較する
        //引数: other 比較対象のシーンID
        //戻り値: ID値の大小関係
        auto operator<=>(const SceneID& other) const = default;

    private:
        std::uint32_t Value = InvalidValue; //シーンID値
    };

    class ObjectID final
    {
    public:
        static constexpr std::uint32_t InvalidValue = 0; //無効なオブジェクトID値

        //無効なオブジェクトIDを作成する
        constexpr ObjectID() noexcept = default;

        //指定値からオブジェクトIDを作成する
        //value : オブジェクトID値
        explicit constexpr ObjectID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //オブジェクトIDが登録済みの有効値か判定する
        //戻り値: ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //オブジェクトID値を比較する
        //引数: other 比較対象のオブジェクトID
        //戻り値: ID値の大小関係
        auto operator<=>(const ObjectID& other) const = default;

    private:
        std::uint32_t Value = InvalidValue; //オブジェクトID値
    };

    class ComponentID final
    {
    public:
        static constexpr std::uint32_t InvalidValue = 0; //無効なコンポーネントID値

        //無効なコンポーネントIDを作成する
        constexpr ComponentID() noexcept = default;

        //指定値からコンポーネントIDを作成する
        //value : コンポーネントID値
        explicit constexpr ComponentID(std::uint32_t value) noexcept
            : Value(value)
        {
        }

        constexpr std::uint32_t GetValue() const noexcept { return Value; }

        //コンポーネントIDが登録済みの有効値か判定する
        //戻り値: ID値が無効値以外の場合はtrue
        constexpr bool IsValid() const noexcept { return Value != InvalidValue; }

        //コンポーネントID値を比較する
        //引数: other 比較対象のコンポーネントID
        //戻り値: ID値の大小関係
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
