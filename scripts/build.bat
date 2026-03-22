@echo off
setlocal enabledelayedexpansion

echo [AURORAOS BUILD SYSTEM]
echo -----------------------

if not exist build mkdir build

where gcc  >nul 2>nul || ( echo [ERROR] GCC not found.  & exit /b 1 )
where nasm >nul 2>nul || ( echo [ERROR] NASM not found. & exit /b 1 )
where ld   >nul 2>nul || ( echo [ERROR] LD not found.   & exit /b 1 )

:: ── 1. Bootloader ─────────────────────────────────────────────────────────
echo [1/4] Building bootloader...
nasm -f bin src\bootloader\bootloader.asm -o build\bootloader.bin
if errorlevel 1 ( echo [ERROR] Bootloader failed & exit /b 1 )
for %%F in (build\bootloader.bin) do echo   bootloader.bin : %%~zF bytes

:: ── 2. Compile all sources ────────────────────────────────────────────────
echo [2/4] Compiling sources...

:: Assemble kernel entry (win32 = COFF object, compatible with MinGW ld)
echo   Assembling kernel_entry.asm...
nasm -f win32 src\bootloader\kernel_entry.asm -o build\kernel_entry.o
if errorlevel 1 ( echo [ERROR] kernel_entry.asm failed & exit /b 1 )

:: GCC flags for a freestanding 32-bit kernel:
::   -m32                  32-bit x86 code
::   -ffreestanding        no libc, no startup files
::   -fno-builtin          don't silently replace memset/memcpy etc.
::   -fno-stack-protector  no __stack_chk_fail needed
::   -mno-stack-arg-probe  no __chkstk_ms needed (MinGW-specific)
::   -fno-pic              no position-independent code
::   -O1                   light optimisation (avoids 64-bit division helpers)
::   -Wall                 all warnings on
set CFLAGS=-m32 -ffreestanding -fno-builtin -fno-stack-protector -mno-stack-arg-probe -fno-pic -O1 -I src -Wall -Wno-implicit-function-declaration

set OBJ_FILES=
set /a IDX=0

for /R src %%f in (*.c) do (
    set /a IDX+=1
    echo   Compiling %%~nxf...
    gcc %CFLAGS% -c "%%f" -o "build\obj_!IDX!_%%~nf.o"
    if errorlevel 1 ( echo [ERROR] Failed: %%~nxf & exit /b 1 )
    set OBJ_FILES=!OBJ_FILES! "build\obj_!IDX!_%%~nf.o"
)

:: ── 3. Link ───────────────────────────────────────────────────────────────
echo [3/4] Linking kernel...

:: kernel_entry.o MUST be listed first — the linker script places its
:: .text section at 0x10000, so _kernel_entry_start is at byte 0 of output.
::
:: We output to kernel.exe (PE format) then strip headers with objcopy.
:: The linker script controls the load address and section order.
ld -m i386pe -T src\kernel\kernel.ld ^
   -o build\kernel.exe ^
   build\kernel_entry.o !OBJ_FILES!
if errorlevel 1 ( echo [ERROR] Linking failed & exit /b 1 )

:: objcopy: convert PE → flat binary.
:: --set-section-flags .bss=alloc,load,contents  forces BSS into the output
:: (otherwise objcopy skips it and the binary is too small / BSS is missing)
objcopy -O binary ^
        --set-section-flags .bss=alloc,load,contents ^
        build\kernel.exe build\kernel.bin
if errorlevel 1 ( echo [ERROR] objcopy failed & exit /b 1 )

:: ── 4. Create disk image ──────────────────────────────────────────────────
echo [4/4] Creating disk image...
copy /b build\bootloader.bin + build\kernel.bin build\auroraos.img >nul
if errorlevel 1 ( echo [ERROR] Image creation failed & exit /b 1 )

for %%F in (build\kernel.bin)   do echo   kernel.bin     : %%~zF bytes
for %%F in (build\auroraos.img) do echo   auroraos.img   : %%~zF bytes

echo -----------------------
echo [SUCCESS] build\auroraos.img is ready.
echo.
echo Run:
echo   qemu-system-x86_64 -drive format=raw,file=build\auroraos.img -m 512M
echo.
echo Debug (log interrupts + resets):
echo   qemu-system-x86_64 -drive format=raw,file=build\auroraos.img -m 512M -d int,cpu_reset -no-reboot 2^>build\qemu.log
echo.
echo Debug (GDB):
echo   qemu-system-x86_64 -drive format=raw,file=build\auroraos.img -m 512M -s -S
echo   gdb build\kernel.exe -ex "set arch i386" -ex "target remote :1234" -ex "break _kernel_main" -ex "c"
