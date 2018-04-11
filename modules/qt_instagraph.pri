QT.instagraph.VERSION = 5.0.0
QT.instagraph.MAJOR_VERSION = 5
QT.instagraph.MINOR_VERSION = 0
QT.instagraph.PATCH_VERSION = 0

QT.instagraph.name = QtInstagraph
QT.instagraph.bins = $$QT_MODULE_BIN_BASE
QT.instagraph.includes = $$QT_MODULE_INCLUDE_BASE $$QT_MODULE_INCLUDE_BASE/QtInstagraph
QT.instagraph.private_includes = $$QT_MODULE_INCLUDE_BASE/QtInstagraph/$$QT.instagraph.VERSION
QT.instagraph.sources = $$QT_MODULE_BASE/src/instagraph
QT.instagraph.libs = $$QT_MODULE_LIB_BASE
QT.instagraph.plugins = $$QT_MODULE_PLUGIN_BASE
QT.instagraph.imports = $$QT_MODULE_IMPORT_BASE
QT.instagraph.depends = core network gui
