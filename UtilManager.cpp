#include <Windows.h>
#include <string>
#include <vector>
#include <cstdio>

#define MAX_MESSAGE 256
static const char* MessageList[MAX_MESSAGE] = { nullptr };

// Vec4構造体
typedef struct { float X, Y, Z, W; } Vec4;
// Char2構造体
typedef struct { const char* First; const char* End; } Char2;
typedef struct { int One; int Second; } Int2;
// ベクター型構造体
typedef struct { Vec4* data; size_t size; size_t capacity; } Vec4Vector;
typedef struct { Char2* data; size_t size; size_t capacity; } Char2Vector;
typedef struct { Int2* data; size_t size; size_t capacity; } Int2Vector;
typedef struct { char** data; size_t size; size_t capacity; } CharVector;
typedef struct { int* data; size_t size; size_t capacity; } IntVector;
typedef struct { float* data; size_t size; size_t capacity; } FloatVector;
typedef struct { bool* data; size_t size; size_t capacity; } BoolVector;
typedef struct { char** keys; size_t size; size_t capacity; } KeyMap;

//===============================
// メッセージ
//===============================
void AddMessage(const char* sent) {
    for (int i = 0; i < MAX_MESSAGE; i++) {
        if (MessageList[i] == nullptr) {
            MessageList[i] = _strdup(sent);
            break;
        }
    }
}
std::wstring ConvertToWString(const char* str)
{
    if (!str) return L"";

    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0); // CP_ACP = ANSI (環境依存)
    if (len == 0) return L"";

    std::wstring wstr(len - 1, L'\0'); // 終端ヌル文字を除いて確保
    MultiByteToWideChar(CP_ACP, 0, str, -1, &wstr[0], len);
    return wstr;
}
const char* ConcatCStr(const char* str1, const char* str2) {
    if (!str1 && !str2) return nullptr;
    if (!str1) return str2;
    if (!str2) return str1;
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    char* result = (char*)malloc(len1 + len2 + 1);
    strcpy_s(result, len1 + 1, str1);
    strcat_s(result, len1 + len2 + 1, str2);
    return result;
}

void ConcatCStrFree(const char* str) {
    free((void*)str);
}

//===============================
// Vec4 系
//===============================
void Vec4_Init(Vec4Vector* vec) {
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
//push_back
void Vec4_PushBack(Vec4Vector* vec, Vec4 value) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        Vec4* new_data = (Vec4*)realloc(vec->data, new_capacity * sizeof(Vec4));
        if (!new_data) {
            AddMessage("\nerror : vector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size] = value;
    vec->size++;
}
//要素の設定
void Vec4_Set(Vec4Vector* vec, size_t index, Vec4 value) {
    if (index >= vec->size) {
        AddMessage("\nerror : vector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = value;
}
//要素を取得
Vec4 Vec4_Get(Vec4Vector* vec, size_t index) {
    if (index >= vec->size) {
        AddMessage("\nerror : vector_get/インデックス範囲外\n");
        return { -1.0f,-1.0f,-1.0f,-1.0f };
    }
    return vec->data[index];
}
//解放
void Vec4_Free(Vec4Vector* vec) {
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//===============================
// Char2 系
//===============================
void Char2_Init(Char2Vector* vec)
{
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
void Char2_PushBack(Char2Vector* vec, Char2 str)
{
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        Char2* new_data = (Char2*)realloc(vec->data, new_capacity * sizeof(Char2));
        if (!new_data) {
            AddMessage("\nerror : char2vector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size] = str;
    vec->size++;
}
void Char2_Set(Char2Vector* vec, size_t index, Char2 str)
{
    if (index >= vec->size) {
        AddMessage("\nerror : char2vector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = str;
}
Char2 Char2_Get(Char2Vector* vec, size_t index)
{
    if (index >= vec->size) {
        AddMessage("\nerror : char2vector_get/インデックス範囲外\n");
        return { 0,0 };
    }
    return vec->data[index];
}
int Char2_GetIndex(Char2Vector* vec, const char* FirstName)
{
    for (size_t i = 0; i < vec->size; i++) {
        if (strcmp(vec->data[i].First, FirstName) == 0) {
            return (int)i; // 見つかった
        }
    }
    return -1; // 見つからなかった
}
void Char2_Free(Char2Vector* vec)
{
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//=============================
// Int2 系
//=============================
void Int2_Init(Int2Vector* vec)
{
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
void Int2_PushBack(Int2Vector* vec, Int2 str)
{
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        Int2* new_data = (Int2*)realloc(vec->data, new_capacity * sizeof(Int2));
        if (!new_data) {
            AddMessage("\nerror : Int2vector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size] = str;
    vec->size++;
}
void Int2_Set(Int2Vector* vec, size_t index, Int2 str)
{
    if (index >= vec->size) {
        AddMessage("\nerror : Int2vector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = str;
}
Int2 Int2_Get(Int2Vector* vec, size_t index)
{
    if (index >= vec->size) {
        AddMessage("\nerror : char2vector_get/インデックス範囲外\n");
        return { 0,0 };
    }
    return vec->data[index];
}
int Int2_GetIndex(Int2Vector* vec, int OneIndex)
{
    for (size_t i = 0; i < vec->size; i++) {
        if (vec->data[i].One == OneIndex) {
            return (int)i; // 見つかった
        }
    }
    return -1; // 見つからなかった
}
void Int2_Free(Int2Vector* vec)
{
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//===============================
// Char 系
//===============================
void VecC_Init(CharVector* vec) {
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
void VecC_PushBack(CharVector* vec, const char* str) {
    //if (!str) return;

    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        char** new_data = (char**)realloc(vec->data, new_capacity * sizeof(const char*));
        if (!new_data) {
            AddMessage("\nerror : charvector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }

    // コピーを確保して保存
    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (!copy) {
        AddMessage("\nerror : charvector_push_back/文字列コピー失敗\n");
        return;
    }
    memcpy(copy, str, len);

    vec->data[vec->size++] = copy;
}
const char* VecC_Get(CharVector* vec, size_t index) {
    if (!vec)
    {
        AddMessage("\nerror : charvector_get/ベクターがNULL\n");
        return NULL;
    }
    if (index >= vec->size) {
        AddMessage("\nerror : charvector_get/インデックス範囲外\n");
        return NULL;
    }
    return vec->data[index];
}
void VecC_Set(CharVector* vec, size_t index, const char* str) {
    if (index >= vec->size) {
        AddMessage("\nerror : charvector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = NULL;

    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    memcpy(copy, str, len);

    vec->data[index] = copy;
}
void VecC_Free(CharVector* vec) {
    for (size_t i = 0; i < vec->size; i++) {
        free((void*)vec->data[i]);
    }
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//===============================
// Int 系
//===============================
void VecInt_Init(IntVector* vec) {
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
void VecInt_PushBack(IntVector* vec, int str) {
    //if (!str) return;

    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        int* new_data = (int*)realloc(vec->data, new_capacity * sizeof(int));
        if (!new_data) {
            AddMessage("\nerror : intvector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }

    vec->data[vec->size++] = str;
}
int VecInt_Get(IntVector* vec, size_t index) {
    if (index >= vec->size) {
        AddMessage("\nerror : intvector_get/インデックス範囲外\n");
        return -1;
    }
    return vec->data[index];
}
void VecInt_Set(IntVector* vec, size_t index, int str) {
    if (index >= vec->size) {
        AddMessage("\nerror : intvector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = str;
}
void VecInt_Free(IntVector* vec) {
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//===============================
// Bool 系
//===============================
void VecBool_Init(BoolVector* vec) {
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}
void VecBool_PushBack(BoolVector* vec, bool str) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 4 : vec->capacity * 2;
        bool* new_data = (bool*)realloc(vec->data, new_capacity * sizeof(bool));
        if (!new_data) {
            AddMessage("\nerror : bool_vector_push_back/メモリの確保に失敗\n");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }

    vec->data[vec->size++] = str;
}
bool VecBool_Get(BoolVector* vec, size_t index) {
    if (index >= vec->size) {
        AddMessage("\nerror : bool_vector_get/インデックス範囲外\n");
        return NULL;
    }
    return vec->data[index];
}
void VecBool_Set(BoolVector* vec, size_t index, bool str) {
    if (index >= vec->size) {
        AddMessage("\nerror : bool_vector_set/インデックス範囲外\n");
        return;
    }
    vec->data[index] = str;
}
void VecBool_Free(BoolVector* vec) {
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

//===============================
// KeyMap 系
//===============================
// KeyMap 関数
void KeyMap_Init(KeyMap* map) {
    map->keys = NULL;
    map->size = 0;
    map->capacity = 0;
}
void KeyMap_Free(KeyMap* map) {
    for (size_t i = 0; i < map->size; i++) {
        free(map->keys[i]);
    }
    free(map->keys);
    map->keys = NULL;
    map->size = 0;
    map->capacity = 0;
}
int KeyMap_EnsureCapacity(KeyMap* map) {
    if (map->size >= map->capacity) {
        size_t new_capacity = (map->capacity == 0) ? 4 : map->capacity * 2;
        char** new_keys = (char**)realloc(map->keys, new_capacity * sizeof(char*));
        if (!new_keys) {
            AddMessage("\nerror : keymap_ensure_capacity/メモリの確保に失敗\n");
            MessageBoxA(NULL, "KeyMap_ErrorMemory", "Error", S_OK);

            return 0; // メモリ確保失敗
        }
        map->keys = new_keys;
        map->capacity = new_capacity;
    }
    return 1; // 成功
}
int KeyMap_Add(KeyMap* map, const char* key) {
    //for (size_t i = 0; i < map->size; i++) {
    //    if (strcmp(map->keys[i], key) == 0) {
    //        printf("error: key '%s' already exists!\n", key);
    //        
    //        //return -1; // 既存
    //    }
    //}
    if (!KeyMap_EnsureCapacity(map)) return -1;

    map->keys[map->size] = _strdup(key);
    return (int)map->size++; // 登録したインデックスを返す
}
int KeyMap_GetIndex(KeyMap* map, const char* key) {
    for (size_t i = 0; i < map->size; i++) {
        if (strcmp(map->keys[i], key) == 0) {
            return (int)i; // 見つかった
        }
    }
    return -1; // 見つからなかった
}
int KeyMap_GetSize(KeyMap* map)
{
    return (int)map->size;
}
void KeyMap_SetKey(KeyMap* map, size_t index, const char* key)
{
    if (index >= map->size) {
        AddMessage("\nerror : KeyMap_set/インデックス範囲外\n");
        return;
    }
    map->keys[index] = NULL;

    size_t len = strlen(key) + 1;
    char* copy = (char*)malloc(len);
    memcpy(copy, key, len);

    map->keys[index] = copy;
}
const char* KeyMap_GetKey(KeyMap* map, int index) {
    if (index < 0 || (size_t)index >= map->size) {
        AddMessage("\nerror : keymap_getkey/インデックス範囲外\n");
        return NULL;
    }
    return map->keys[index];
}