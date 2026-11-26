module game.world.terrain;

namespace craftbuild {
    auto WorldGenerationContext::top_y() const -> i32 { return min_y + height - 1; }

    auto RandomSource::next_int(i32 bound) -> i32 {
        if (bound <= 0) throw std::invalid_argument("RandomSource::next_int bound must be positive");
        auto distribution = std::uniform_int_distribution<i32>(0, bound - 1);
        return distribution(generator);
    }

    auto RandomSource::next_int(i32 min_inclusive, i32 max_inclusive) -> i32 {
        if (min_inclusive > max_inclusive) return min_inclusive;
        auto distribution = std::uniform_int_distribution<i32>(min_inclusive, max_inclusive);
        return distribution(generator);
    }

    auto VerticalAnchor::absolute(i32 y) -> VerticalAnchor { return { VerticalAnchorType::ABSOLUTE, y }; }
    auto VerticalAnchor::above_bottom(i32 offset) -> VerticalAnchor { return { VerticalAnchorType::ABOVE_BOTTOM, offset }; }
    auto VerticalAnchor::below_top(i32 offset) -> VerticalAnchor { return { VerticalAnchorType::BELOW_TOP, offset }; }

    auto VerticalAnchor::resolve_y(WorldGenerationContext const& context) const -> i32 {
        switch (type) {
        case VerticalAnchorType::ABSOLUTE:     return value;
        case VerticalAnchorType::ABOVE_BOTTOM: return context.min_y + value;
        case VerticalAnchorType::BELOW_TOP:    return context.top_y() - value;
        }

        return value;
    }

    HeightProvider::~HeightProvider() = default;

    ConstantHeight::ConstantHeight(VerticalAnchor value) : value(value) {}

    auto ConstantHeight::of(VerticalAnchor value) -> HeightProviderPtr { return std::make_unique<ConstantHeight>(value); }
    auto ConstantHeight::get_value() const -> VerticalAnchor const& { return value; }
    auto ConstantHeight::sample(RandomSource&, WorldGenerationContext const& context) const -> i32 { return value.resolve_y(context); }
    auto ConstantHeight::get_type() const -> HeightProviderType { return HeightProviderType::CONSTANT; }

    UniformHeight::UniformHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive) : min_inclusive(min_inclusive), max_inclusive(max_inclusive) {}

    auto UniformHeight::of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive) -> HeightProviderPtr { return std::make_unique<UniformHeight>(min_inclusive, max_inclusive); }
    
    auto UniformHeight::sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 {
        auto const min_y = min_inclusive.resolve_y(context);
        auto const max_y = max_inclusive.resolve_y(context);

        if (min_y > max_y) return min_y;
        return random.next_int(min_y, max_y);
    }

    auto UniformHeight::get_type() const -> HeightProviderType { return HeightProviderType::UNIFORM; }

    BiasedToBottomHeight::BiasedToBottomHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner) : min_inclusive(min_inclusive), max_inclusive(max_inclusive), inner(std::max<i32>(1, inner)) {}

    auto BiasedToBottomHeight::of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner) -> HeightProviderPtr { return std::make_unique<BiasedToBottomHeight>(min_inclusive, max_inclusive, inner); }

    auto BiasedToBottomHeight::sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 {
        auto const min_y = min_inclusive.resolve_y(context);
        auto const max_y = max_inclusive.resolve_y(context);
        auto const bound = max_y - min_y - inner + 1;
        if (bound <= 0) return min_y;

        auto const offset = random.next_int(bound);
        return random.next_int(offset + inner) + min_y;
    }

    auto BiasedToBottomHeight::get_type() const -> HeightProviderType { return HeightProviderType::BIASED_TO_BOTTOM; }

    VeryBiasedToBottomHeight::VeryBiasedToBottomHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner) : min_inclusive(min_inclusive), max_inclusive(max_inclusive), inner(std::max<i32>(1, inner)) {}
    
    auto VeryBiasedToBottomHeight::of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 inner) -> HeightProviderPtr { return std::make_unique<VeryBiasedToBottomHeight>(min_inclusive, max_inclusive, inner); }
    
    auto VeryBiasedToBottomHeight::sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 {
        auto const min_y = min_inclusive.resolve_y(context);
        auto const max_y = max_inclusive.resolve_y(context);
        if (max_y - min_y - inner + 1 <= 0) return min_y;

        auto const first = random.next_int(min_y + inner, max_y);
        auto const second = random.next_int(min_y, first - 1);
        return random.next_int(min_y, second - 1 + inner);
    }

    auto VeryBiasedToBottomHeight::get_type() const -> HeightProviderType { return HeightProviderType::VERY_BIASED_TO_BOTTOM; }

    TrapezoidHeight::TrapezoidHeight(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 plateau) : min_inclusive(min_inclusive), max_inclusive(max_inclusive), plateau(plateau) {}

    auto TrapezoidHeight::of(VerticalAnchor min_inclusive, VerticalAnchor max_inclusive, i32 plateau) ->HeightProviderPtr { return std::make_unique<TrapezoidHeight>(min_inclusive, max_inclusive, plateau); }

    auto TrapezoidHeight::sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 {
        auto const min_y = min_inclusive.resolve_y(context);
        auto const max_y = max_inclusive.resolve_y(context);
        if (min_y > max_y) return min_y;

        auto const rng = max_y - min_y;
        if (plateau >= rng) return random.next_int(min_y, max_y);

        auto const first_range = (rng - plateau) / 2;
        auto const second_range = rng - first_range;
        return min_y + random.next_int(0, second_range) + random.next_int(0, first_range);
    }

    auto TrapezoidHeight::get_type() const -> HeightProviderType { return HeightProviderType::TRAPEZOID; }

    WeightedListHeight::WeightedListHeight(std::vector<Entry> distribution) : distribution(std::move(distribution)) {
        for (auto const& entry : this->distribution) {
            if (entry.provider and entry.weight > 0) total_weight += entry.weight;
        }
    }

    auto WeightedListHeight::of(std::vector<Entry> distribution) -> HeightProviderPtr { return std::make_unique<WeightedListHeight>(std::move(distribution)); }

    auto WeightedListHeight::sample(RandomSource& random, WorldGenerationContext const& context) const -> i32 {
        if (total_weight <= 0) return 0;

        auto chosen = random.next_int(total_weight);
        for (auto const& entry : distribution) {
            if (not entry.provider or entry.weight <= 0) continue;
            if (chosen < entry.weight) return entry.provider->sample(random, context);
            chosen -= entry.weight;
        }

        return 0;
    }

    auto WeightedListHeight::get_type() const -> HeightProviderType { return HeightProviderType::WEIGHTED_LIST; }
}