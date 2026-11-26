export module game.block.normal_blocks;

import std;

import misc.string;
import misc.number;
import game.block;
import game.block.block_data;

export namespace craftbuild {
	struct Air final : public Block1F {};
	struct Dirt final : public Block1F {};
	struct Grass final : public Block3F {};
	struct Stone final : public Block1F {};
	struct Pebble final : public Block1F {};
	struct OakLog final : public Block3F {};
	struct OakPlanks final : public Block1F {};
	struct OakLeaves final : public Block1F { auto init_tags() -> std::unordered_set<u32> override final { return { tag_registry::get_id("transparent") }; } };
	struct DiamondBlock final : public Block1F {};
	struct DiamondOre final : public Block1F {};
	struct Bedrock final : public Block1F {};
}