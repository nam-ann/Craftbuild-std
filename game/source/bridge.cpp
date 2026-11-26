#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/engine.hpp>
ENABLE_WARNING

import misc.pos;
import misc.string;
import misc.number;
import game.core;
import game.main;
import game.server;
import game.player;
import game.logger;
import game.thread;
import game.environment;

using namespace godot;
using namespace craftbuild;

auto initialize_module(ModuleInitializationLevel p_level) -> void {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    log<LogType::VERBOSE>("Hello from the DLL!");
    log<LogType::VERBOSE>("Game version: {}"f(FULL_VERSION));

    ClassDB::register_class<Main>();
    ClassDB::register_class<Player>();
    ClassDB::register_class<Sun>();
    ClassDB::register_class<CraftSky>();
}

auto initialize_server(ModuleInitializationLevel p_level) -> void {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

	log<LogType::VERBOSE>("Hello from the Server DLL!");
    log<LogType::VERBOSE>("Game version: {}"f(FULL_VERSION));
    
	ClassDB::register_class<Server>();
}

auto uninitialize_module(ModuleInitializationLevel p_level) -> void {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

auto uninitialize_server(ModuleInitializationLevel p_level) -> void {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {
    auto GDE_EXPORT craftbuild_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) -> GDExtensionBool {
        thread_registry::register_thread("Main");

        auto init_obj = GDExtensionBinding::InitObject(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_module);
        init_obj.register_terminator(uninitialize_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
    auto GDE_EXPORT craftbuild_server(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization) -> GDExtensionBool {
        thread_registry::register_thread("Main");

        auto init_obj = GDExtensionBinding::InitObject(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_server);
        init_obj.register_terminator(uninitialize_server);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        return init_obj.init();
    }
}