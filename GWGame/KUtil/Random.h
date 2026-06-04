#pragma once

#ifndef RANDOM_GENERATOR_H
#define RANDOM_GENERATOR_H

#include <random>
#include <vector>
#include <stdexcept>

class RandomGenerator
{
public:
    // シングルトンインスタンスの取得
    static RandomGenerator& GetInstance()
    {
        static RandomGenerator instance;
        return instance;
    }

    // コピー・ムーブ禁止
    RandomGenerator(const RandomGenerator&) = delete;
    RandomGenerator& operator=(const RandomGenerator&) = delete;

    // ========== 完全ランダム ==========

    // int: [min, max] の範囲でランダム
    int RandInt(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(m_engine);
    }

    // float: [min, max] の範囲でランダム
    float RandFloat(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(m_engine);
    }

    // double: [min, max] の範囲でランダム
    double RandDouble(double min, double max)
    {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(m_engine);
    }

    // ========== リストからランダム選択 ==========

    template <typename T> T PickFrom(const std::vector<T>& values)
    {
        if (values.empty())
            throw std::invalid_argument("値リストが空です");
        std::uniform_int_distribution<size_t> dist(0, values.size() - 1);
        return values[dist(m_engine)];
    }

private:
    RandomGenerator() : m_engine(std::random_device{}())
    {
    }

    std::mt19937 m_engine;

    
};

// 省略記法マクロ
#define RNG RandomGenerator::getInstance()

#endif // RANDOM_GENERATOR_H