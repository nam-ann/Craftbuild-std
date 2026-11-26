export module game.world.terrain;

import std;
import misc.number;

export namespace craftbuild {
    struct WorldGenerationContext final {
        i32 min_y = 0dw;
        i32 height = 255dw;

        auto top_y() const -> i32;
    };

    class RandomSource final {
    private:
        std::mt19937 generator;

    public:
        explicit RandomSource(u32 seed) : generator(seed) {}

        auto next_int(i32 bound) -> i32;
        auto next_int(i32 min_inclusive, i32 max_inclusive) -> i32;
    };

    enum class VerticalAnchorType { ABSOLUTE, ABOVE_BOTTOM, BELOW_TOP, };

    struct VerticalAnchor final {
        VerticalAnchorType type = VerticalAnchorType::ABSOLUTE;
        i32 value = 0;

        static auto absolute(i32 y) -> VerticalAnchor;
        static auto above_bottom(i32 offset) -> VerticalAnchor;
        static auto below_top(i32 offset) -> VerticalAnchor;

        auto resolve_y(WorldGenerationContext const& context) const -> i32;
    };

    enum class HeightProviderType { CONSTANT, UNIFORM, BIASED_TO_BOTTOM, VERY_BIASED_TO_BOTTOM, TRAPEZOID, WEIGHTED_LIST };

    class HeightProvider {
    public:
        virtual ~HeightProvider();

        virtual auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 = 0;
        virtual auto get_type() const -> HeightProviderType = 0;
    };

    using HeightProviderPtr = std::unique_ptr<const HeightProvider>;

    class ConstantHeight final : public HeightProvider {
    private:
        VerticalAnchor value;

    public:
        explicit ConstantHeight(VerticalAnchor value);

        static auto of(VerticalAnchor value) -> HeightProviderPtr;

        auto get_value() const -> VerticalAnchor const&;
        auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 override final;
        auto get_type() const -> HeightProviderType override final;
    };

    class UniformHeight final : public HeightProvider {
    private:
        VerticalAnchor min_inclusive;
        VerticalAnchor max_inclusive;

    public:
        UniformHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive);

        static auto of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive) -> HeightProviderPtr;

        i32 sample(RandomSource& random, WorldGenerationContext const& context) const override final;
        HeightProviderType get_type() const override final;
    };

    class BiasedToBottomHeight final : public HeightProvider {
    private:
        VerticalAnchor min_inclusive;
        VerticalAnchor max_inclusive;
        i32 inner = 1;

    public:
        BiasedToBottomHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner = 1);

        static auto of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner = 1) -> HeightProviderPtr;

        auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 override final;
        auto get_type() const -> HeightProviderType override final;
    };

    class VeryBiasedToBottomHeight final : public HeightProvider {
    private:
        VerticalAnchor min_inclusive;
        VerticalAnchor max_inclusive;
        i32 inner = 1;

    public:
        VeryBiasedToBottomHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner = 1);

        static auto of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner = 1) -> HeightProviderPtr;

        auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 override final;
        auto get_type() const -> HeightProviderType override final;
    };

    class TrapezoidHeight final : public HeightProvider {
    private:
        VerticalAnchor min_inclusive;
        VerticalAnchor max_inclusive;
        i32 plateau = 0;

    public:
        TrapezoidHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 plateau = 0);

        static auto of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 plateau = 0) -> HeightProviderPtr;

        auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 override final;
        auto get_type() const -> HeightProviderType override final;
    };

    class WeightedListHeight final : public HeightProvider {
    public:
        struct Entry {
            HeightProviderPtr provider;
            i32 weight = 1;
        };

    private:
        std::vector<Entry> distribution;
        i32 total_weight = 0;

    public:
        explicit WeightedListHeight(std::vector<Entry> distribution);

        static HeightProviderPtr of(std::vector<Entry> distribution);

        auto sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 override final;
        auto get_type() const -> HeightProviderType override final;
    };
}
