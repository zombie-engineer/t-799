$(info importing rules.mk)
MAKEFLAGS += --no-builtin-rules

$(BUILD_DIR)/kernel8.bin: $(BUILD_DIR)/kernel8.elf $(BUILD_DIR)/os.json
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/kernel8.elf: $(OBJS) $(LINKER_SCRIPT_PATH)
	echo ddd >> $(BUILD_DIR)/build_cmd.log
	cd $(BUILD_DIR) && $(LD) $(OBJS_BUILD_REL) -o kernel8.elf -T $(LINKER_SCRIPT_PATH) -Map=kernel8.map -print-memory-usage

$(BUILD_DIR)/%.o: %.c
	mkdir -vp $(dir $@)
	echo $(CC) $(CFLAGS) -c $< -o $@ >> $(BUILD_DIR)/build_cmd.log
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	mkdir -vp $(dir $@)
	echo $(CC) $(ASFLAGS) -g -c $< -o $@ >> $(BUILD_DIR)/build_cmd.log
	$(CC) $(ASFLAGS) -g -c $< -o $@

$(OBJ_DIRS):
	@echo Recepie for $(OBJ_DIRS)
	mkdir -p $(OBJ_DIRS)

.PHONY: .force

$(BUILD_DIR)/task_offsets.o: t-799/src/task_offsets.c .force
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/task_offsets.bin: $(BUILD_DIR)/task_offsets.o
	$(OBJCOPY) -O binary --only-section .task_struct_layout $< $@

$(BUILD_DIR)/os.json: $(BUILD_DIR)/kernel8.elf $(BUILD_DIR)/task_offsets.bin
	CROSSCOMPILE=$(CROSSCOMPILE) \
	ELFFILE=$(BUILD_DIR)/kernel8.elf \
	BINFILE=$(BUILD_DIR)/task_offsets.bin \
	TO_JSON=$@ \
	. $(CORE_ROOT)/scripts/gen_gdb_json.sh
	cat $@

