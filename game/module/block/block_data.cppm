export module game.block.block_data;

import std;

import misc.string;
import misc.number;

export namespace craftbuild {
	namespace meta_registry {
		inline auto registry = std::vector<std::string>{};
		inline auto name2id = std::unordered_map<std::string, u32, string_hash, std::equal_to<>>();

		auto register_metadata(std::string_view name) -> void;
		auto get_metadata(u32 meta_id) -> std::string&;
		auto get_id(std::string_view meta_name) -> u32;
		auto has_metadata(std::string_view meta_name) -> bool;
	}

	namespace tag_registry {
		inline auto registry = std::vector<std::string>{};
		inline auto name2id = std::unordered_map<std::string, u32, string_hash, std::equal_to<>>();

		auto register_tag(std::string_view name) -> void;
		auto get_tag(u32 tag_id) -> std::string&;
		auto get_id(std::string_view tag_name) -> u32;
		auto has_tag(std::string_view tag_name) -> bool;
	}
}