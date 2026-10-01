# Namida: Date added and Date modified sorting logic

This is an implementation handoff for an agent reproducing Namida's date sorting behavior. It describes observed source behavior, including quirks; suggested improvements are explicitly separated. Pseudocode below is an independent, language-neutral description rather than copied application code.

- Repository: https://github.com/namidaco/namida
- Reviewed branch: `main`
- Reviewed commit: `c38ed074dd2537b2d00fc416dc9ecd344aa22258`
- Review date: 2026-10-01
- Scope: library tracks/videos, grouped media, playlist items, playlist metadata, Recently added, and file browser date sorting.
- Verification: static tracing of the checked-out source. The Flutter application was not built or run. External dependencies were not cloned, so dependency-specific behavior is marked where relevant.

## 1. Start with the correct meaning of each date

| Context | Added/created date key | Modified date key |
|---|---|---|
| Library track or video | `TrackExtended.dateAdded`: file-derived or provider-derived timestamp | `TrackExtended.dateModified`: file/provider modification timestamp |
| Album, artist, genre and similar grouped media | `getDateAddedEffective()` over contained tracks | `getDateModifiedEffective()` over contained tracks |
| Item inside a local playlist | `TrackWithDate.dateAddedMS`: time that item was added to the playlist | Underlying `track.dateModified` |
| Playlist list/card | `GroupSortType.creationDate`: playlist's own creation date | `GroupSortType.modifiedDate`: playlist's own modification date |
| Recently added view | Secondary key: library track's `dateAdded` | **Primary key: library track's `dateModified`** |
| File browser | No added-date sort in `FileBrowserSortType` | File/directory `modifiedMS`, then lowercase name |

Do not merge `dateModified` (track/group enum) and `modifiedDate` (playlist metadata enum). Do not substitute a track's file-derived added date for its playlist-entry date.

The normal file-stat and remote-provider paths store Unix epoch **milliseconds** as integers. Display formatting is separate from sorting. The optional Android MediaStore path directly assigns dependency values; its units require verification in that dependency (see section 8).

## 2. How local-library timestamps are obtained

### Normal Dart file-stat path

`Indexer.convertTagToTrack()` uses supplied `FileStatsAdv` if present; otherwise it calls `File(trackPath).stat()` and wraps the result with `FileStatsAdv.fromFileStat()`.

The fields become:

```text
track.dateAdded    = stats.creationDateMS if present, otherwise 0
track.dateModified = stats.modifiedMS     if present, otherwise 0
```

`creationDateMS` is computed by Namida's `FileStatsUtils.creationDate` extension. It is an estimate based on three file-stat timestamps, not simply a birth-time field and not the time Namida indexed the file.

```text
thresholdMicroseconds = epochMicroseconds(localDateTime(1980, 1, 1)) + 1
valid = [stat.modified, stat.changed, stat.accessed]
        filtered to timestamps strictly greater than thresholdMicroseconds

if valid is nonempty:
    estimatedAddedDate = earliest valid timestamp
else:
    estimatedAddedDate = localDateTime(1970, 1, 1)

creationDateMS = epochMilliseconds(estimatedAddedDate)
modifiedMS    = epochMilliseconds(stat.modified)
```

Important precision details:

- The validity test is strictly `>`; it is not `>=`.
- Validation happens in microseconds before conversion to milliseconds in this path.
- `DateTime(1980)` and `DateTime(1970)` are local-time constructors in the code. The no-valid-date fallback is therefore not universally integer zero; its epoch value depends on timezone.
- A stat acquisition failure is caught; absent stats lead to zero-valued track dates.
- The modification date is used directly without applying the 1980 cutoff.
- An old modification time can make an imported/copied file appear old under Date added, even if it just entered the library.

### Windows native directory-listing path

`DirsFileFilter` first tries the native Windows walker when enabled; on failure it falls back to the Dart walker.

The native walker reads last-write, creation and last-access `FILETIME` values. Its added-date estimate is the earliest valid value among those three, using a millisecond cutoff:

```text
thresholdMS = epochMilliseconds(localDateTime(1980, 1, 1)) + 1
valid = [lastWriteMS, creationMS, lastAccessMS] where value > thresholdMS
creationDateMS = min(valid) if any exist, otherwise 0
modifiedMS = lastWriteMS
```

Native conversion is `(FILETIME - 116444736000000000) ~/ 10000`, where `~/` truncates division toward zero; nonpositive input returns zero. Native all-invalid fallback is zero, unlike the Dart local-1970 fallback. Native creation time also differs from Dart's `stat.changed` candidate.

The scan packs each file's stats in `[size, modifiedMS, creationMS]` order, then reconstructs `FileStatsAdv` before indexing.

## 3. Ordinary track sorting

The key resolvers map directly:

```text
Date added    -> track.dateAdded
Date modified -> track.dateModified
```

The comparator is numeric, using the full stored integer:

```text
compare(a, b, key, reverse):
    if reverse:
        return numericCompare(key(b), key(a))
    return numericCompare(key(a), key(b))
```

- `reverse = false`: ascending / oldest first.
- `reverse = true`: descending / newest first.
- Date keys have no implicit negation and no implicit newest-first behavior.
- Missing dates represented by zero sort before positive timestamps ascending and after them descending. The sorter does not special-case unknown dates or replace them with the current time.
- Library track reverse settings default to false.
- Choosing a date key does not itself reset or toggle the existing reverse setting.

### Simple sort versus advanced multiple-key sort

The ordinary track menu passes `forceSingleSorting: true`. It saves a one-element sort-key list and sorts with that key. Equal dates have no automatic filename/title fallback.

Advanced sorting reads the ordered list in `settings.mediaItemsTrackSorting[media]`. Keys are compared lexicographically: compare the first, then use the next only if equal. For example, `[dateModified, dateAdded, title]` means modified date first, added date second, title third. A single reverse flag applies to the entire comparison, including all secondary keys.

`getMediaTracksSortingComparables()` constructs the key extractors. `Indexer.sortMediaTracksSubLists()` delegates to `LibraryGroup.sortAllSync()`, which sorts the main list and applicable media sublists.

### Ties and performance

`sortByPrecomputed` and `sortByAltsPrecomputed` behave as follows:

- Fewer than two items: no work.
- Fewer than 16 items: directly compare extracted keys with `List.sort`.
- At least 16 items: precompute each key per item, sort indices by those keys, then apply the reordered items.
- No final original-index, ID, title or filename tie-breaker is appended.
- Fully equal keys return comparator result zero. Do not promise stable ordering: the implementation uses `List.sort` and makes no stability guarantee.

A different helper, `sortedByPrecomputed`, explicitly adds an original-index tie-breaker. That helper's stability behavior must not be attributed to the in-place helpers used by the ordinary date sorts.

### Search results

`sortTracksSearch()` returns immediately when automatic/relevance sorting is enabled. When manual sorting is enabled, it applies the same date-key sort to `trackSearchTemp` and persists `tracksSortSearch` and `tracksSortSearchReversed`. These are separate from library sort settings.

## 4. Recently added is modified-first

`Indexer.recentlyAddedTracksSorted()` copies the current track list and applies:

```text
sort keys = [track.dateModified, track.dateAdded]
reverse = true
```

Equivalent comparator:

```text
compareRecentlyAdded(a, b):
    result = numericCompare(b.dateModified, a.dateModified)
    if result != 0:
        return result
    return numericCompare(b.dateAdded, a.dateAdded)
```

This means most recently modified first, with most recently added resolving equal modification times. It does not sort primarily by added date. The function returns a sorted copy and does not change the user's ordinary library sort settings. It is used by the home page and Recently added route.

## 5. Grouped media: effective dates

Albums, artists, genres and similar group sorts use each group's contained tracks.

### Effective added date: reproduce the initialization quirk

```text
effectiveAddedDate(tracks):
    if tracks is empty:
        return null
    best = tracks[0].dateAdded
    thresholdMS = epochMilliseconds(localDateTime(1980, 1, 1)) + 1
    for track in tracks:
        candidate = track.dateAdded
        if candidate < best and candidate > thresholdMS:
            best = candidate
    return best
```

The initial `best` is not validated. If the first track has date zero or another too-old value, later valid dates cannot replace it because they are larger. Thus this is **not generally equivalent to min(valid dates)**, and the result can depend on the current track order.

### Effective modified date

```text
effectiveModifiedDate(tracks):
    if tracks is empty:
        return null
    return maximum(track.dateModified for track in tracks)
```

Group sort resolvers turn a null effective date into zero. They compare those numeric keys with the configured group sort direction and optional secondary group keys. Display resolvers separately format the effective dates.

## 6. Playlists: entry dates versus playlist metadata

### Items inside a playlist

`PlaylistController.onPlaylistItemsSort()` special-cases `SortType.dateAdded`:

```text
playlist entry Date added key = entry.dateAddedMS
playlist entry Date modified key = entry.track.dateModified
```

Other keys delegate to the ordinary track key resolver. Multiple entry-sort keys are compared in order; reverse applies to the whole comparator. This path calls external `dart_extensions` helpers `sortByAlts` / `sortByReverseAlts`; this review does not establish dependency-specific stability guarantees.

`TrackWithDate` persists its own `dateAdded`. Its JSON reader uses the current time when that field is absent, unlike `TrackExtended`, which uses zero. Playlist import/add methods receive entry dates through callbacks; favorites explicitly construct an entry with `currentTimeMS`. Some bulk/import paths assign incrementing timestamps to successive items.

### Sorting playlist cards

The playlist list uses `GroupSortType.creationDate` and `GroupSortType.modifiedDate`, reading the playlist object's own numeric fields. The local playlist comparator returns no key for `GroupSortType.dateAdded` or `GroupSortType.dateModified`. After sorting the list, pinned playlists are moved first, so pinning can override the visible global date order.

The default local playlist sort-key list is `[GroupSortType.dateModified]`, while that enum returns no comparator in `_getPlaylistSortingComparable`. Consequently, that default alone does not perform a date comparison in this source snapshot. Do not silently describe it as a working modified-date default or replace it with `modifiedDate` without identifying the change.

Playlist mutation timestamp generation is partly owned by the external `playlist_manager` package and is outside the verified dependency scope here.

## 7. File browser modified-date sorting

Both files and directories compare `modifiedMS` ascending, then their lowercase names ascending for equal timestamps. Reversing swaps comparator arguments, so it also reverses the name tie-breaker.

```text
compareBrowserEntries(a, b, reverse):
    if reverse:
        swap a and b
    result = numericCompare(a.modifiedMS, b.modifiedMS)
    if result != 0:
        return result
    return stringCompare(a.nameLower, b.nameLower)
```

These entry timestamps come from `stat.modified.millisecondsSinceEpoch`. This name fallback belongs to the file browser; ordinary library track date sorting does not add it.

## 8. Provider-specific and refresh behavior

| Source | Added date | Modified date | Notes |
|---|---|---|---|
| Dart local file scan | Oldest valid modified/changed/accessed timestamp | File modified timestamp | Local-1970 fallback for all-invalid added-date candidates |
| Native Windows scan | Oldest valid last-write/creation/access timestamp | Last-write timestamp | Zero fallback; milliseconds threshold |
| WebDAV | `file.cTime` converted to milliseconds | `file.mTime` converted to milliseconds | Missing field becomes zero during track conversion |
| Jellyfin | `item.dateCreated` converted to milliseconds | `item.dateModified` converted to milliseconds | Missing field becomes zero |
| Subsonic | `media.created` converted to milliseconds | Also `media.created` converted to milliseconds | This adapter does not supply an independent modification date |
| Optional Android MediaStore | `e.dateAdded ?? 0` | `e.dateModified ?? 0` | Assigned directly from `on_audio_query`; no visible seconds-to-milliseconds conversion in Namida's assignment. Verify units in the configured fork before reproducing or correcting. |

Local refresh considers an existing file unchanged only when both its size matches and its modification timestamps match after truncation to seconds (`storedDateModified ~/ 1000 == scannedModifiedMS ~/ 1000`). Therefore a same-size modification within the same second is not detected by this check. Unknown scan stats cause the existing file to be skipped by this comparison. Added date is not itself a diff trigger.

Tag editing re-stats a successfully updated track and refreshes its stored size and `dateModified`. A keep-file-dates option is passed to the tag writer; the refreshed value is whatever the file reports afterward. `copyWithTag` preserves the existing track `dateAdded`. A full reindex can derive timestamps again from file/provider stats rather than preserving a historical first-seen time.

Persistence: `TrackExtended` JSON reads missing dates as zero and writes date fields only when positive. Negative or zero dates can therefore be omitted and reload as zero. Date formatting should never be used as the comparison key; no date-only truncation is applied by the ordinary sort.

## 9. Agent implementation checklist and acceptance examples

Implement separate timestamp concepts and numeric comparators before connecting the UI:

1. Store file/provider added and modified timestamps separately from playlist-entry added time.
2. Implement the local estimate and Windows variant as specified, including threshold and fallback differences if exact compatibility is required.
3. Map ordinary date sorts to raw numeric keys. Persist the chosen key(s) and reverse flag separately.
4. Apply ordered secondary keys only when explicitly configured. Do not invent a title fallback for library date sorting.
5. Implement Recently added as modified descending, then added descending.
6. Implement effective group dates with the initialization quirk if preserving behavior.
7. Keep playlist metadata enum names separate from track/group enum names.
8. Keep browser name tie-breaking separate from library comparators.
9. Verify provider units at the boundary; retain timestamps as numbers until display.

Use this small fixture; values are abstract timestamps for comparator checks:

| Track | dateAdded | dateModified |
|---|---:|---:|
| A | 100 | 500 |
| B | 200 | 400 |
| C | 150 | 500 |
| D | 0 | 0 |

Expected results:

- Date added ascending: `D, A, C, B`.
- Date added descending: `B, C, A, D`.
- Date modified ascending: `D, B`, then `A/C` in unspecified relative order.
- Recently added: `C, A, B, D`.
- A playlist with A entry date 900 and B entry date 800 sorts `B, A` by playlist Date added ascending, even though their library Date added ascending is `A, B`.
- Group modified date for `[A, B, C]`: 500.
- Group added-date quirk: first date zero followed by a valid 2026 date returns zero; reversing those two tracks returns the valid date. Use real post-cutoff timestamps for this test.
- Browser entries with equal modification dates compare by lowercase name, descending names when reversed.

If improving behavior rather than cloning it, explicitly decide whether to use a true first-seen library timestamp, fix the group invalid-first-date behavior, make ties stable, use UTC cutoffs, normalize MediaStore units, and correct the playlist default enum mismatch. These are changes to Namida's observed behavior, not part of the extraction.

## 10. Source references pinned to the reviewed commit

The key functions and ranges are linked below so an agent can inspect the exact snapshot:

- [Track models, playlist-entry dates and JSON persistence](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/class/track.dart): `TrackWithDate` lines 25–88; `TrackExtended` date fields and JSON/copy methods.
- [File date estimation](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/extensions.dart#L1130-L1145): `FileStatsUtils.creationDate`.
- [Effective group dates](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/extensions.dart#L105-L129): `getDateAddedEffective`, `getDateModifiedEffective`.
- [In-place precomputed comparators](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/extensions.dart#L375-L428) and [index ordering](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/extensions.dart#L517-L527).
- [Indexer](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/indexer_controller.dart): `recentlyAddedTracksSorted` lines 139–150; `convertTagToTrack` lines 751 onward; `getPathsDifference` lines 2296 onward; `_fetchMediaStoreTracks` lines 2376 onward; `FileStatsAdv` lines 2806–2824.
- [Dart directory stats collection](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/dirs_file_filter.dart) and [Windows timestamp conversion/selection](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/core/dirs_file_filter.windows.dart#L200-L214).
- [Search and sorting controller](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/search_sort_controller.dart): group date comparables lines 302–303; track comparables lines 356–357; ordered media track keys lines 415–430; library/search sorts lines 1121–1259; playlist list sorting and metadata comparables lines 1483–1566.
- [Shared library list sorting](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/class/library_group.dart#L106-L174): `sortAllSync`.
- [Playlist entry sort override](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/playlist_controller.dart#L1201-L1223): `onPlaylistItemsSort`.
- [Sort settings/defaults](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/settings_controller.dart) and [simple menu selecting single-key mode](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/ui/widgets/sort_by_button.dart#L20-L55).
- [File browser comparators](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/file_browser.dart#L1579-L1661): `_FileEntry`, `_DirEntry`.
- [Tag edit timestamp refresh](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/tagger_controller.dart#L216-L224).
- Provider adapters: [WebDAV](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/music_web_server/webdav_server.dart), [Jellyfin](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/music_web_server/jellyfin_server.dart), [Subsonic](https://github.com/namidaco/namida/blob/c38ed074dd2537b2d00fc416dc9ecd344aa22258/lib/controller/music_web_server/subsonic_web_server.dart).
