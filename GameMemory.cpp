module;

#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstring>

export module GameMemory;

import GameAddresses;

struct DeepPointer {
    uintptr_t base;
    std::vector<uintptr_t> offsets;

    DeepPointer(uintptr_t b, std::initializer_list<uintptr_t> offs) : base(b), offsets(offs) {}

    // Helper: read a pointer-sized value from target process and return it in 'out'.
    // Uses gameAddresses.ptrSize (set in GameAddresses) to choose 4/8-byte reads.
    static bool readTargetPtr(HANDLE hProcess, uintptr_t srcAddr, uintptr_t &out) {
        if (!srcAddr) return false;
        if (gameAddresses.ptrSize == 8) {
            uint64_t tmp = 0;
            if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(srcAddr), &tmp, sizeof(tmp), nullptr)) return false;
            out = static_cast<uintptr_t>(tmp);
            return true;
        } else {
            uint32_t tmp = 0;
            if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(srcAddr), &tmp, sizeof(tmp), nullptr)) return false;
            out = static_cast<uintptr_t>(tmp);
            return true;
        }
    }

    // LiveSplit-style: add offset then deref all but last (kept consistent)
    uintptr_t resolveLiveSplit(HANDLE hProcess) const {
        uintptr_t addr = base;
        for (size_t i = 0; i < offsets.size(); ++i) {
            addr += offsets[i];
            if (i + 1 < offsets.size()) {
                uintptr_t tmp = 0;
                if (!readTargetPtr(hProcess, addr, tmp)) {
                    return 0;
                }
                addr = tmp;
            }
        }
        return addr;
    }

    // New strategy: dereference current address first, then add offset
    uintptr_t resolveDerefFirst(HANDLE hProcess) const {
        uintptr_t addr = base;
        for (size_t i = 0; i < offsets.size(); ++i) {
            uintptr_t tmp = 0;
            if (!readTargetPtr(hProcess, addr, tmp)) {
                return 0;
            }
            addr = tmp + offsets[i];
        }
        return addr;
    }

    // Existing: add offset, dereference all but last (using readTargetPtr)
    uintptr_t resolve(HANDLE hProcess) const {
        uintptr_t addr = base;
        for (size_t i = 0; i < offsets.size(); ++i) {
            addr += offsets[i];
            if (i + 1 < offsets.size()) {
                uintptr_t tmp = 0;
                if (!readTargetPtr(hProcess, addr, tmp)) {
                    return 0;
                }
                addr = tmp;
            }
        }
        return addr;
    }



    // Read raw bytes at resolved address.
    bool resolveBytes(HANDLE hProcess, void* out, size_t len) const {
        if (!out || len == 0) return false;

        struct Candidate { uintptr_t addr; const char* name; };
        Candidate cands[3];

        cands[0].addr = resolve(hProcess);            cands[0].name = "resolve(add-then-deref)";
        cands[1].addr = resolveLiveSplit(hProcess);  cands[1].name = "resolveLiveSplit";
        cands[2].addr = resolveDerefFirst(hProcess); cands[2].name = "resolveDerefFirst";

        for (const auto& c : cands) {
            if (!c.addr) continue;

            if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(c.addr), out, len, nullptr)) {
                return true;
            }

            uintptr_t ptr = 0;
            if (readTargetPtr(hProcess, c.addr, ptr) && ptr) {
                if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(ptr), out, len, nullptr)) {
                    return true;
                }
            }
        }

        return false;
    }
};

// Snapshot of memory state
export struct GameMemorySnapshot_s {
    bool    loading     = false;
    bool    prompt      = false;
    unsigned char focusState = 0; // CurMap Workaround: 1 = focused and ingame, 2 = focused and main menu or mapchange trigger or cutscene
    bool    isPaused    = false;
    float   sync        = 0.0f;
    float   globalTimer = 0.0f;
    // char    CurMap[21]  = {0};
    char    End[6]      = {0};
    char    EndRaw[6]   = {0}; // raw bytes before sanitization

};

export GameMemorySnapshot_s snapShotCurrent;
export GameMemorySnapshot_s snapShotPrevious;


// Offsets for the current game version
export struct VersionOffsets_s {
    uintptr_t loading       = 0x0;
    uintptr_t prompt        = 0x0;
    uintptr_t focusState    = 0x0;
    uintptr_t isPaused      = 0x0;
    uintptr_t sync          = 0x0;
    uintptr_t globalTimer   = 0x0;
    // DeepPointer CurMap      = DeepPointer(gameAddresses.xrCore   + 0x0, {0x0,0x0,0x0,0x0,0x0,0x0});
    DeepPointer End         = DeepPointer(gameAddresses.baseAddr + 0x0, {0x0,0x0,0x0,0x0,0x0,0x0,0x0});

    // Bulk-read block for tightly packed timer-related fields
    uintptr_t blockBase     = 0x0;
    size_t    blockSize     = 0;
    size_t    offFocusState = 0;
    size_t    offIsPaused   = 0;
    size_t    offSync       = 0;
    size_t    offGlobal     = 0;

    // Maximum bulk block size (covers both supported versions)
    static constexpr size_t kMaxBulkBlock = 0x2000;

    float syncLowerBound    = 0.0f;
    float syncUpperBound    = 0.0f;

} versionOffsets;


export void setupVersionOffsets() {

    setupGameAddresses();


    if (gameAddresses.baseSize == 1662976 || gameAddresses.baseSize == 1613824) {
        // 1.0000
        versionOffsets.loading      = gameAddresses.xrNetServer + 0xFAC4;
        versionOffsets.prompt       = gameAddresses.xrGame      + 0x54C2F9;
        versionOffsets.focusState   = gameAddresses.baseAddr    + 0x10300C;
        versionOffsets.isPaused     = gameAddresses.baseAddr    + 0x1047C0;
        versionOffsets.sync         = gameAddresses.baseAddr    + 0x104928;
        versionOffsets.globalTimer  = gameAddresses.baseAddr    + 0x10492C;
        // versionOffsets.CurMap       = DeepPointer(gameAddresses.xrCore + 0xBA040, {0x4,0x0,0x40,0x8,0x20,0x14});
        versionOffsets.End          = DeepPointer(gameAddresses.baseAddr + 0x1048BC, {0x54,0x14,0x0,0x0,0x44,0xC,0x12});

        versionOffsets.syncLowerBound = 0.057f;
        versionOffsets.syncUpperBound = 0.11f;

    } else {
         // 1.0006
        versionOffsets.loading      = gameAddresses.xrNetServer + 0x13E84;
        versionOffsets.prompt       = gameAddresses.xrGame      + 0x560668;
        versionOffsets.focusState   = gameAddresses.baseAddr    + 0x10A10C;
        versionOffsets.isPaused     = gameAddresses.baseAddr    + 0x10BCD0;
        versionOffsets.sync         = gameAddresses.baseAddr    + 0x10BE80;
        versionOffsets.globalTimer  = gameAddresses.baseAddr    + 0x10BE84;
        // versionOffsets.CurMap       = DeepPointer(gameAddresses.xrCore + 0xBF368, {0x4,0x0,0x40,0x8,0x28,0x4});
        versionOffsets.End          = DeepPointer(gameAddresses.baseAddr + 0x10BDB0, {0x3C,0x10,0x0,0x0,0x44,0xC,0x12});

        versionOffsets.syncLowerBound = 0.09f;
        versionOffsets.syncUpperBound = 0.11f;

    }

    // Compute bulk-read block (covers focusState/isPaused/sync/globalTimer)
    {
        const uintptr_t a1 = versionOffsets.focusState;
        const uintptr_t a2 = versionOffsets.isPaused;
        const uintptr_t a3 = versionOffsets.sync;
        const uintptr_t a4 = versionOffsets.globalTimer;

        const uintptr_t minAddr = std::min(std::min(a1, a2), std::min(a3, a4));
        const uintptr_t maxEnd =
            std::max(std::max(a1 + sizeof(snapShotCurrent.focusState),
                              a2 + sizeof(snapShotCurrent.isPaused)),
                     std::max(a3 + sizeof(snapShotCurrent.sync),
                              a4 + sizeof(snapShotCurrent.globalTimer)));

        versionOffsets.blockBase     = minAddr;
        versionOffsets.blockSize     = static_cast<size_t>(maxEnd - minAddr);
        versionOffsets.offFocusState = static_cast<size_t>(a1 - minAddr);
        versionOffsets.offIsPaused   = static_cast<size_t>(a2 - minAddr);
        versionOffsets.offSync       = static_cast<size_t>(a3 - minAddr);
        versionOffsets.offGlobal     = static_cast<size_t>(a4 - minAddr);
    }
}

static bool isPrintableAscii(const char* s, size_t maxlen) {
    if (!s || maxlen == 0) return false;
    size_t len = 0;
    for (; len < maxlen && s[len] != '\0'; ++len) {
        unsigned char c = static_cast<unsigned char>(s[len]);
        if (c < 0x20 || c > 0x7E) return false;
    }
    return len > 0;
}

export void readGameMemorySnapshot() {

    ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.loading),
        &snapShotCurrent.loading, sizeof(snapShotCurrent.loading), nullptr);

    ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.prompt),
        &snapShotCurrent.prompt, sizeof(snapShotCurrent.prompt), nullptr);

    // Bulk read focusState/isPaused/sync/globalTimer in one RPM call
    {
        static thread_local unsigned char block[VersionOffsets_s::kMaxBulkBlock];
        if (versionOffsets.blockBase &&
            versionOffsets.blockSize > 0 &&
            versionOffsets.blockSize <= VersionOffsets_s::kMaxBulkBlock &&
            ReadProcessMemory(gameAddresses.hProcess,
                              reinterpret_cast<LPCVOID>(versionOffsets.blockBase),
                              block,
                              versionOffsets.blockSize,
                              nullptr)) {

            std::memcpy(&snapShotCurrent.focusState,
                        block + versionOffsets.offFocusState,
                        sizeof(snapShotCurrent.focusState));

            std::memcpy(&snapShotCurrent.isPaused,
                        block + versionOffsets.offIsPaused,
                        sizeof(snapShotCurrent.isPaused));

            std::memcpy(&snapShotCurrent.sync,
                        block + versionOffsets.offSync,
                        sizeof(snapShotCurrent.sync));

            std::memcpy(&snapShotCurrent.globalTimer,
                        block + versionOffsets.offGlobal,
                        sizeof(snapShotCurrent.globalTimer));
        } else {
            // Fallback to individual reads if bulk RPM fails
            ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.isPaused),
                &snapShotCurrent.isPaused, sizeof(snapShotCurrent.isPaused), nullptr);

            ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.sync),
                &snapShotCurrent.sync, sizeof(snapShotCurrent.sync), nullptr);

            ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.globalTimer),
                &snapShotCurrent.globalTimer, sizeof(snapShotCurrent.globalTimer), nullptr);

            ReadProcessMemory(gameAddresses.hProcess, reinterpret_cast<LPCVOID>(versionOffsets.focusState),
                &snapShotCurrent.focusState, sizeof(snapShotCurrent.focusState), nullptr);
        }
    }

    // End - always capture raw 5 bytes, but suppress verbose debug unless requested
    {
        char raw[5] = {0};
        bool gotRaw = false;

        if (versionOffsets.End.resolveBytes(gameAddresses.hProcess, raw, sizeof(raw))) {
            gotRaw = true;
        }

        // Always update EndRaw buffer (NUL-terminate for safe logging)
        memset(snapShotCurrent.EndRaw, 0, sizeof(snapShotCurrent.EndRaw));

        if (gotRaw) {

            memcpy(snapShotCurrent.EndRaw, raw, sizeof(raw));
            snapShotCurrent.EndRaw[5] = 0;

        }

        // sanitize to printable ASCII into snapShotCurrent.End (kept for other code/legacy)
        if (gotRaw && isPrintableAscii(raw, sizeof(raw))) {

            memcpy(snapShotCurrent.End, raw, sizeof(raw));
            snapShotCurrent.End[5] = 0;

        } else snapShotCurrent.End[0] = 0;

    }

}
