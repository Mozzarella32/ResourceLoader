#pragma once

struct GlslangRAII {
    GlslangRAII();
    ~GlslangRAII();

    GlslangRAII(const GlslangRAII &) noexcept = delete;
    GlslangRAII(GlslangRAII &&) noexcept = delete;
    auto operator=(const GlslangRAII &) noexcept = delete;
    auto operator=(GlslangRAII &&) noexcept = delete;
};
