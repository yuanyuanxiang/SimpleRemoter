@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

:: SimpleRemoter Test Management Script
:: Usage: test.bat [build|run|clean|rebuild|help] [options]
::
:: Test Phases:
::   Phase 1: Buffer + Protocol
::   Phase 2: File Transfer
::   Phase 3: Network
::   Phase 4: Screen/Image

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "CONFIG=Release"

set "ACTION=%~1"
set "OPTION=%~2"

if "%ACTION%"=="" goto help
if "%ACTION%"=="build" goto build
if "%ACTION%"=="run" goto run
if "%ACTION%"=="clean" goto clean
if "%ACTION%"=="rebuild" goto rebuild
if "%ACTION%"=="help" goto help

echo Error: Unknown action "%ACTION%"
echo.
goto help

:build
echo ========================================
echo  Building Tests (14 executables)
echo ========================================

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/2] Configuring CMake...
    cmake -B "%BUILD_DIR%" -S "%SCRIPT_DIR%"
    if errorlevel 1 (
        echo Error: CMake configuration failed
        exit /b 1
    )
) else (
    echo [1/2] CMake already configured, skipping...
)

echo [2/2] Compiling tests...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo Error: Build failed
    exit /b 1
)

echo.
echo Build successful! (14 test executables)
echo.
echo Phase 1 - Buffer/Protocol:
echo   - client_buffer_test.exe (33 tests)
echo   - server_buffer_test.exe (40 tests)
echo   - protocol_test.exe (58 tests)
echo.
echo Phase 2 - File Transfer:
echo   - file_transfer_test.exe (37 tests)
echo   - chunk_manager_test.exe (36 tests)
echo   - sha256_verify_test.exe (28 tests)
echo   - resume_state_test.exe (26 tests)
echo.
echo Phase 3 - Network:
echo   - header_test.exe (29 tests)
echo   - packet_fragment_test.exe (24 tests)
echo   - http_mask_test.exe (27 tests)
echo.
echo Phase 4 - Screen/Image:
echo   - diff_algorithm_test.exe (32 tests)
echo   - rgb565_test.exe (286 tests)
echo   - scroll_detector_test.exe (43 tests)
echo   - quality_adaptive_test.exe (64 tests)
exit /b 0

:run
echo ========================================
echo  Running Tests
echo ========================================

if not exist "%BUILD_DIR%\%CONFIG%\client_buffer_test.exe" (
    echo Tests not built, building first...
    call :build
    if errorlevel 1 exit /b 1
    echo.
)

if "%OPTION%"=="client" goto run_client
if "%OPTION%"=="server" goto run_server
if "%OPTION%"=="protocol" goto run_protocol
if "%OPTION%"=="file" goto run_file
if "%OPTION%"=="network" goto run_network
if "%OPTION%"=="screen" goto run_screen
if "%OPTION%"=="verbose" goto run_verbose
goto run_all

:run_client
echo Running client Buffer tests [33]...
"%BUILD_DIR%\%CONFIG%\client_buffer_test.exe" --gtest_color=yes
goto check_result

:run_server
echo Running server Buffer tests [40]...
"%BUILD_DIR%\%CONFIG%\server_buffer_test.exe" --gtest_color=yes
goto check_result

:run_protocol
echo Running protocol tests [58]...
"%BUILD_DIR%\%CONFIG%\protocol_test.exe" --gtest_color=yes
goto check_result

:run_file
echo Running file transfer tests [127]...
"%BUILD_DIR%\%CONFIG%\file_transfer_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\chunk_manager_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\sha256_verify_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\resume_state_test.exe" --gtest_color=yes
goto check_result

:run_network
echo Running network tests [80]...
"%BUILD_DIR%\%CONFIG%\header_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\packet_fragment_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\http_mask_test.exe" --gtest_color=yes
goto check_result

:run_screen
echo Running screen/image tests [425]...
"%BUILD_DIR%\%CONFIG%\diff_algorithm_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\rgb565_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\scroll_detector_test.exe" --gtest_color=yes
"%BUILD_DIR%\%CONFIG%\quality_adaptive_test.exe" --gtest_color=yes
goto check_result

:run_verbose
echo Running all tests [verbose]...
ctest --test-dir "%BUILD_DIR%" -C %CONFIG% -V
goto check_result

:run_all

echo Running all tests...
ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure

:check_result
if errorlevel 1 (
    echo.
    echo Tests FAILED!
    exit /b 1
)
echo.
echo All tests PASSED!
goto end

:clean
echo ========================================
echo  Cleaning Build
echo ========================================

if exist "%BUILD_DIR%" (
    echo Removing build directory...
    rmdir /s /q "%BUILD_DIR%"
    echo Clean complete!
) else (
    echo Build directory does not exist
)
exit /b 0

:rebuild
echo ========================================
echo  Rebuilding
echo ========================================
call :clean
echo.
call :build
goto end

:help
echo.
echo SimpleRemoter Test Management Script
echo ========================================
echo.
echo Usage: test.bat ^<command^> [options]
echo.
echo Commands:
echo   build           Build all 14 test executables
echo   run             Run all tests
echo   run client      Run client Buffer tests
echo   run server      Run server Buffer tests
echo   run protocol    Run protocol tests
echo   run file        Run file transfer tests
echo   run network     Run network tests
echo   run screen      Run screen/image tests
echo   run verbose     Run all tests with verbose output
echo   clean           Clean build directory
echo   rebuild         Clean and rebuild
echo   help            Show this help
echo.
echo Test Phases:
echo   Phase 1: Buffer + Protocol
echo   Phase 2: File Transfer
echo   Phase 3: Network
echo   Phase 4: Screen/Image
echo.
echo Examples:
echo   test.bat build          # Build all tests
echo   test.bat run            # Run all 763 tests
echo   test.bat run screen     # Run Phase 4 screen tests
echo   test.bat rebuild        # Clean and rebuild
echo.
goto end

:end
endlocal
