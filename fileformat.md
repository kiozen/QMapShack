# QMS Binary File Format V2

QMapShack uses a binary format to store all GIS data in a single binary
blob. The format is used:

* To exchange GIS data via file between instances of QMapShack.
* To store the history of an item
* To store a complete copy of an GIS item in the database

The binary data is compressed to save memory. The data is structured in a tree.
Branches and leaves of the tree can contain version information. The version
defines how to read the data.

The binary format is upward compatible but not downward compatible. In other
words: A larger version can read data from a lower version but not vice versa.
This is a major caveat of the format.

The new version of the format should solve that problem for future versions.

The rough solution is:

* Each data item is preceded by a type enumeration.
* Data items that are not atomic types with a well known size are preceded with
  a length value.

By that the reader can identify what data is to be read skipping data that is
not known. In the case of a skip the item has to be flagged as reduced and a
warning has to be shown when writing the data in the reduced format.

## Serialization

QMapShack V2 binaries are stored little endian. For they use Qt datastream
protocol Qt_5_2 for serialization.

## Sections

A QMapShack binary has one or several main sections. These sections are:

* Project data
* Waypoint data
* Track data
* Route data
* Area data

Those sections contain structured data of any kind. Each section starts
with 10 bytes of a magic string (no \0 termation). The magic strings are:

```
#define MAGIC_TRK       "QMTrkV2   "
#define MAGIC_WPT       "QMWptV2   "
#define MAGIC_RTE       "QMRteV2   "
#define MAGIC_AREA      "QMAreaV2  "
#define MAGIC_PROJ      "QMProjV2  "
```

The magic string is followed by an 8 bit version number. The version
number is followed by the 32 bit section size. The section size is
the number of bytes to follow the section size.

The section data itself is a QByteArray with compressed data. It's
using Qt's compression algorithm `qCompress()` with compression level 9.

## Data storage

Data can be divided in to major groups. Atomic data that covers all basic C++
with a well known size. Complex data with a size not determined by it's type.

Type:

* Common to all data types is the type specifier.

Size:

* Atomic data types do not need a size information as it it is implicitly defined.
* Complex data types must have a size information.

Serialization is: Type, Size, Data

```
0xTTTT, 0xSSSSSSSS, 0xD1, 0xD2, ...
```

### Type specifier

The type specifier is a 16 bit unsigned integer.


### Data size

The data size information is an unsigned 32 bit integer.

## Reading data

The reader will parse the data along the tree and read the data according
to their well known type specifier. If a type specifier is not known
the data should be skipped and a data skipped flag must be set.

## Writing data

If the data skipped flag is set the user must confirm to save the data with
loosing information. The data is saved according to the implemented type
handlers.

## Appending data structures

Usually new data fields can be added without any other change as each data
has it's own unique type specifier. If there is a major break in how to
handle the data field, the version number of the section has to be increased.


IRtRecord, CRtGpsTetherRecord, CRtOpenSkyRecord