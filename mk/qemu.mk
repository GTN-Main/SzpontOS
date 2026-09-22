# SzpontOS QEMU Emulator Execution Rules
# Included by root Makefile

.PHONY: run run-virtio run-virgl run-ps2 run-usb run-stress run-cli debug

# Run in graphical QEMU
run: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE)

# Run in graphical QEMU with Virtio-VGA
run-virtio: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --virtio

# Run in graphical QEMU with Virtio-GPU 3D Virgl Acceleration
run-virgl: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --virgl

# Run in graphical QEMU with Bare Metal PS/2 simulation
run-ps2: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --baremetal-ps2

# Run in graphical QEMU with Pure USB xHCI (UEFI Modern Bare Metal)
run-usb: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --baremetal-usb

# Run with realistic timing and instruction cycle stress test
run-stress: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --timing-stress

# Run in headless QEMU (terminal only)
run-cli: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --headless

# Run in graphical QEMU with GDB debug stub
debug: build $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --debug
