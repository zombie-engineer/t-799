$(info importing rules.mk)

# This removes any builtin rules from make, so no unexpected builds
MAKEFLAGS += --no-builtin-rules

$(BUILD_DIR)/kernel8.bin: $(BUILD_DIR)/kernel8.elf $(BUILD_DIR)/os.json
	$(call run,$(OBJCOPY) -O binary $< $@)

$(BUILD_DIR)/kernel8.elf: $(OBJS) $(LINKER_SCRIPT_PATH)
	$(call run,cd $(BUILD_DIR) && $(LD) $(OBJS_BUILD_REL) -o kernel8.elf -T $(LINKER_SCRIPT_PATH) -Map=kernel8.map -print-memory-usage)

$(BUILD_DIR)/%.o: %.c
	@mkdir -vp $(dir $@)
	$(call run,$(CC) $(CFLAGS) -c $< -o $@)

$(BUILD_DIR)/%.o: %.S
	@mkdir -vp $(dir $@)
	$(call run,$(CC) $(ASFLAGS) -g -c $< -o $@)

.PHONY: .force

$(BUILD_DIR)/task_offsets.o: t-799/src/task_offsets.c .force
	$(call run,$(CC) $(CFLAGS) -c $< -o $@)

$(BUILD_DIR)/task_offsets.bin: $(BUILD_DIR)/task_offsets.o
	$(call run,$(OBJCOPY) -O binary --only-section .task_struct_layout $< $@)

$(BUILD_DIR)/os.json: $(BUILD_DIR)/kernel8.elf $(BUILD_DIR)/task_offsets.bin
	$(call run,CROSSCOMPILE=$(CROSSCOMPILE) \
	  ELFFILE=$(BUILD_DIR)/kernel8.elf \
	  BINFILE=$(BUILD_DIR)/task_offsets.bin \
	  TO_JSON=$@ \
	. $(CORE_ROOT)/scripts/gen_gdb_json.sh)
	cat $@
