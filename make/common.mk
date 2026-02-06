CMD_LOG_FILE := $(BUILD_DIR)/build_cmd.log

# runs a command from recipe first logging it to a command log file
define run
  @echo "$(1)" >> $(CMD_LOG_FILE)
  @$(1)
endef

