#include "stdafx.h"
#include "../MusicPlayer2/Define.h"
#include "../MusicPlayer2/Common.h"
#include "../MusicPlayer2/SongInfo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest
{
    TEST_CLASS(DateSorting)
    {
    public:
        TEST_METHOD(DateKeysAndRecentlyAddedMatchNamidaFixture)
        {
            std::vector<SongInfo> tracks(4);
            const __int64 added[] = { 100, 200, 150, 0 };
            const __int64 modified[] = { 500, 400, 500, 0 };
            for (int i = 0; i < 4; ++i)
            {
                tracks[i].title = std::wstring(1, L'A' + i);
                tracks[i].date_added_ms = added[i];
                tracks[i].date_modified_ms = modified[i];
            }
            auto sortedNames = [&](SortMode mode) {
                auto copy = tracks;
                std::stable_sort(copy.begin(), copy.end(), SongInfo::GetSortFunc(mode));
                std::wstring names;
                for (const auto& song : copy)
                    names += song.title;
                return names;
            };
            Assert::AreEqual(L"DACB", sortedNames(SM_U_ADDED).c_str());
            Assert::AreEqual(L"BCAD", sortedNames(SM_D_ADDED).c_str());
            Assert::AreEqual(L"DBAC", sortedNames(SM_U_TIME).c_str());
            Assert::AreEqual(L"ACBD", sortedNames(SM_D_TIME).c_str());
            Assert::AreEqual(L"CABD", sortedNames(SM_RECENT_ADDED).c_str());
            for (auto mode : { SM_U_TIME, SM_D_TIME })
            {
                auto less = SongInfo::GetSortFunc(mode);
                Assert::IsFalse(less(tracks[0], tracks[2]));
                Assert::IsFalse(less(tracks[2], tracks[0]));
                Assert::IsFalse(less(tracks[0], tracks[0]));
            }
        }

        TEST_METHOD(WindowsDatesUseEarliestValidTimestampAndZeroOnFailure)
        {
            struct TempFile
            {
                wchar_t path[MAX_PATH]{};
                TempFile()
                {
                    wchar_t directory[MAX_PATH]{};
                    Assert::IsTrue(GetTempPathW(MAX_PATH, directory) != 0);
                    Assert::IsTrue(GetTempFileNameW(directory, L"mp2", 0, path) != 0);
                }
                ~TempFile() { DeleteFileW(path); }
            } file;
            auto setDates = [&](const FILETIME& creation, const FILETIME& access, const FILETIME& write) {
                HANDLE handle = CreateFileW(file.path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                Assert::IsTrue(handle != INVALID_HANDLE_VALUE);
                BOOL success = SetFileTime(handle, &creation, &access, &write);
                CloseHandle(handle);
                Assert::IsTrue(success != FALSE);
            };
            auto date = [](WORD year, WORD milliseconds) {
                SYSTEMTIME time{};
                time.wYear = year;
                time.wMonth = time.wDay = 1;
                time.wMilliseconds = milliseconds;
                FILETIME result{};
                SystemTimeToFileTime(&time, &result);
                return result;
            };
            setDates(date(2026, 3), date(2026, 2), date(2026, 1));
            __int64 added{}, modified{};
            Assert::IsTrue(CCommon::GetFileTrackDates(file.path, added, modified));
            Assert::AreEqual(modified, added);
            std::vector<SongInfo> tracks(2);
            tracks[0].file_path = file.path;
            tracks[1].file_path = L"";
            SongInfo::SortSongs(tracks, SM_U_ADDED);
            Assert::IsTrue(tracks[0].file_path.empty());
            SongInfo::SortSongs(tracks, SM_RECENT_ADDED);
            Assert::AreEqual(file.path, tracks[0].file_path.c_str());
            setDates(date(2026, 3), date(2026, 2), date(1975, 1));
            Assert::IsTrue(CCommon::GetFileTrackDates(file.path, added, modified));
            Assert::IsTrue(added > modified);
            setDates(date(1975, 3), date(1975, 2), date(1975, 1));
            Assert::IsTrue(CCommon::GetFileTrackDates(file.path, added, modified));
            Assert::AreEqual<__int64>(0, added);
            added = modified = 123;
            Assert::IsFalse(CCommon::GetFileTrackDates(L"", added, modified));
            Assert::AreEqual<__int64>(0, added);
            Assert::AreEqual<__int64>(0, modified);
        }
    };
}
