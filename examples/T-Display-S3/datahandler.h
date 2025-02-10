#ifndef DATAHANDLER_H
#define DATAHANDLER_H

#include <Arduino.h>
#include <vector>
#include <variant>
#include <string>
#include <unordered_map>

// ------------------- TIPOS DE DADOS -------------------



// 🔹 Define um elemento que pode ser um número, string ou vetor de strings
using TypeElement = std::variant<int, double, float, std::string, std::vector<std::string>>;

// 🔹 Define um vetor de elementos (linha de uma tabela)
using TypeVector = std::vector<TypeElement>;

// 🔹 Define uma tabela (coleção de linhas)
using TypeTable = std::vector<TypeVector>;

// 🔹 Define um "cubo" (conjunto de tabelas, para manipulação avançada de dados)
using TypeCube = std::vector<TypeTable>;




// ------------------- CLASSE ELEMENT -------------------

struct Element {
    TypeElement value;

    // 🔹 Construtor Padrão
    Element() : value("") {}

    // 🔹 Construtor Genérico para `TypeElement`
    Element(const TypeElement& v) : value(v) {}

    // 🔹 Construtor Específico para `std::string`
    Element(const std::string& str) : value(str) {}

    // 🔹 Construtor Específico para `std::vector<std::string>`
    Element(const std::vector<std::string>& vec) : value(vec) {}

    // 🔹 Converte para TypeElement
    operator const TypeElement&() const { return value; }

    // 🔹 Retorna um elemento dentro de um vetor (se for um vetor)
    std::string operator[](size_t index) const {
        if (std::holds_alternative<std::vector<std::string>>(value)) {
            return std::get<std::vector<std::string>>(value)[index];
        }
        Serial.println("❌ ERRO: O elemento não é um vetor.");
        return "";
    }

    // 🔹 Verifica se o Element contém um vetor de strings
    bool isVector() const {
        return std::holds_alternative<std::vector<std::string>>(value);
    }

    // 🔹 Obtém o vetor de strings se o Element for um vetor
    std::vector<std::string> getVector() const {
        if (isVector()) {
            return std::get<std::vector<std::string>>(value);
        }
        Serial.println("❌ ERRO: O elemento não contém um vetor de strings.");
        return {};
    }
};



// // ------------------- CLASSE VECTOR -------------------



struct Vector {
    TypeVector values;

    // 🔹 Construtor Padrão
    Vector() = default;

    // 🔹 Construtor Recebe `TypeVector`
    Vector(TypeVector v) : values(v) {}

    // 🔹 Acesso a um Element diretamente
    Element operator[](size_t index) {
        return Element(values[index]);
    }

    // 🔹 Acesso a subelementos dentro de vetores
    std::string operator()(size_t index, size_t subIndex) {
        if (index < values.size() && std::holds_alternative<std::vector<std::string>>(values[index])) {
            return std::get<std::vector<std::string>>(values[index])[subIndex];
        }
        Serial.println("❌ ERRO: Tentativa de acessar um índice inválido ou elemento não é um vetor.");
        return "";
    }

    // 🔹 Conversão automática para `TypeVector`
    operator TypeVector&() { return values; }

    // 🔹 Conversão automática para `const TypeVector&`
    operator const TypeVector&() const { return values; }
};




// // ------------------- CLASSE TABLE -------------------



struct Table {
    TypeTable data;
    std::vector<std::string> columnNames;
    std::unordered_map<std::string, int> columnIndex;  // Nome → Índice da Coluna
    std::unordered_map<std::string, int> rowNameToIndex;  // Nome → Índice da Linha

    // 🔹 Construtor Padrão
    Table() = default;

    // 🔹 Construtor que recebe `TypeTable`
    Table(TypeTable v, std::vector<std::string> colNames = {}) : data(v), columnNames(colNames) {
        for (size_t i = 0; i < columnNames.size(); ++i) {
            columnIndex[columnNames[i]] = i;
        }
    }

    // 🔹 Retorna uma linha pelo índice como `Vector`
    Vector row(int rowIndex) const {
        return Vector(data[rowIndex]);
    }

    // 🔹 Retorna uma linha pelo nome, permitindo busca por coluna específica
    Vector row(const std::string& rowName, const std::string& by = "rowNameToIndex") const {
        // 🔹 Se `by == "rowNameToIndex"`, usamos `rowNameToIndex` (se disponível)
        if (by == "rowNameToIndex" && rowNameToIndex.find(rowName) != rowNameToIndex.end()) {
            return Vector(data[rowNameToIndex.at(rowName)]);
        }

        // 🔹 Busca pelo nome dentro de uma coluna específica
        if (columnIndex.find(by) != columnIndex.end()) {
            int colIdx = columnIndex.at(by);
            for (size_t i = 0; i < data.size(); ++i) {
                if (std::holds_alternative<std::string>(data[i][colIdx]) &&
                    std::get<std::string>(data[i][colIdx]) == rowName) {
                    return Vector(data[i]);
                }
            }
        }

        // ❌ Se não encontrar, exibe erro e retorna vetor vazio
        Serial.print("❌ ERRO: Linha com '");
        Serial.print(rowName.c_str());
        Serial.print("' na coluna '");
        Serial.print(by.c_str());
        Serial.println("' não encontrada.");

        return Vector();
    }

    // 🔹 Retorna uma coluna pelo índice como `Vector`
    Vector column(int colIndex) const {
        TypeVector col;
        for (const auto& row : data) {
            col.push_back(row[colIndex]);
        }
        return Vector(col);
    }

    // 🔹 Retorna uma coluna pelo nome como `Vector`
    Vector column(const std::string& colName) const {
        if (columnIndex.find(colName) != columnIndex.end()) {
            return column(columnIndex.at(colName));
        }
        Serial.print("❌ ERRO: Coluna '");
        Serial.print(colName.c_str());
        Serial.println("' não encontrada.");
        return Vector();
    }

    // 🔹 Permite acessar uma linha diretamente com `ThePath[0]`
    Vector operator[](size_t rowIndex) const {
        return Vector(data[rowIndex]);  // ✅ Retorna `Vector`, ou seja, uma linha da tabela
    }

        // 🔹 Adiciona uma nova linha de dados à tabela
    void addRow(const TypeVector& row) {
        if (!columnNames.empty() && row.size() != columnNames.size()) {
            Serial.println("❌ ERRO: O número de elementos da linha não corresponde ao número de colunas!");
            return;
        }
        data.push_back(row);
    }

};




// ------------------- FUNÇÕES AUXILIARES -------------------

// 🔹 Converte um array estático para um TypeVector
template <typename T, size_t N>
TypeVector ToVector(const T (&arr)[N]) {
    TypeVector result;
    result.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        result.push_back(arr[i]);
    }
    return result;
}

// 🔹 Rotaciona um vetor e retorna uma tabela
inline TypeTable spin(const TypeVector& arr) {
    size_t N = arr.size();
    TypeTable rotated(N, TypeVector(N));
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            rotated[i][j] = arr[(j + i) % N];
        }
    }
    return rotated;
}

// 🔹 Retorna uma linha específica de um vetor rotacionado
inline TypeVector spin(const TypeVector& arr, size_t idx) {
    size_t N = arr.size();
    TypeVector rotated(N);
    for (size_t j = 0; j < N; ++j) {
        rotated[j] = arr[(j + idx) % N];
    }
    return rotated;
}

// 🔹 Retorna um array repetido como tabela
inline TypeTable repeatarray(const TypeVector& arr) {
    size_t N = arr.size();
    TypeTable repeated(N, TypeVector(N));
    for (size_t i = 0; i < N; ++i) {
        repeated[i] = arr;
    }
    return repeated;
}

// 🔹 Mantendo a versão original de `spread()`
inline TypeTable spread(const TypeVector& arr) {
    size_t N = arr.size();
    TypeTable spreadArray(N);
    for (size_t i = 0; i < N; ++i) {
        spreadArray[i] = TypeVector(N, arr[i]);
    }
    return spreadArray;
}

// 🔹 Nova sobrecarga para aceitar índice extra
inline TypeVector spread(const TypeVector& arr, size_t idx) {
    size_t N = arr.size();
    if (idx >= N) {
        Serial.println("⚠️ ERRO: Índice fora do limite em `spread()`.");
        return TypeVector();
    }
    return TypeVector(N, arr[idx]);
}


// 🔹 Rotaciona um TypeVector com base em um elemento específico
inline TypeVector rotate(const TypeVector& vec, const TypeElement& elem, size_t n = 0, const std::string& mode = "value") {
    TypeVector rotated;
    size_t idx = vec.size();

    if (n == 0) {
        n = vec.size();
    }

    if (mode == "idx" && std::holds_alternative<int>(elem)) {
        idx = std::get<int>(elem);
    } else if (mode == "value") {
        auto it = std::find(vec.begin(), vec.end(), elem);
        if (it != vec.end()) {
            idx = std::distance(vec.begin(), it);
        }
    }

    if (idx < vec.size()) {
        for (size_t i = 0; i < n; ++i) {
            rotated.push_back(vec[(i + idx) % vec.size()]);
        }
    }

    return rotated;
}

// 🔹 Converte um índice de linha para o nome correspondente
inline std::string RowNameByIndex(const std::unordered_map<std::string, int>& rowNameToIndex, int targetIndex) {
    for (const auto& pair : rowNameToIndex) {
        if (pair.second == targetIndex) {
            return pair.first;
        }
    }
    return "";
}

// 🔹 Retorna se um TypeElement é int ou string
inline const char* intOrString(const TypeElement& value) {
    if (std::holds_alternative<int>(value)) {
        return "int";
    } else if (std::holds_alternative<std::string>(value)) {
        return "string";
    }
    return "unknown";
}

#endif // DATAHANDLER_H
