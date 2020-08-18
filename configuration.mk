
# Set some initial BOB variables
BOB_SDK_DIRECTORY          ?= ./
BOB_OUTPUT_BASE_DIRECTORY  ?= output
BOB_OUTPUT_DIRECTORY           := $(BOB_OUTPUT_BASE_DIRECTORY)/$(BOB_PROJECT)
BOB_OUTPUT_BINARY_DIRECTORY    := $(BOB_OUTPUT_DIRECTORY)/binary
BOB_OUTPUT_COMPONENT_DIRECTORY := $(BOB_OUTPUT_DIRECTORY)/components
BOB_GENERATED_FILE_DIRECTORY   := $(BOB_OUTPUT_DIRECTORY)/generated
BOB_BUILD_SYSTEM_DIRECTORY := $(BOB_SDK_DIRECTORY)/tools/mbob
BOB_MAKEFILE_DIRECTORY     := $(BOB_BUILD_SYSTEM_DIRECTORY)/makefiles

BOB_FEATURE_IDENTIFIER       :=+
BOB_COMMAND_IDENTIFIER       :=!
BOB_COMMAND_TARGET_SEPARATOR :=@

BOB_DEFAULT_COMPONENTS     :=

BOB_COMPONENT_DIRECTORIES := $(BOB_SDK_DIRECTORY) \
                             tools/toolchains \
                             applications \
                             components
