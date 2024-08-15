#include "../JPSql/Model/Logger.hpp"
#include "../JPSql/Model/Record.hpp"
#include "../JPSql/Model/Relation.hpp"
#include "../JPSql/Model/Utils.hpp"
#include "../JPSql/SqlConnection.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

struct Artist;
struct Track;
struct Publisher;

struct Artist: Model::Record<Artist>
{
    Model::Field<std::string, 2, "name"> name;
    Model::HasMany<Track, "artist_id"> tracks;

    Artist():
        Record { "artists" },
        name { *this },
        tracks { *this }
    {
    }
};

struct Track: Model::Record<Track>
{
    Model::Field<std::string, 2, "title"> title;
    Model::BelongsTo<Artist, 3, "artist_id"> artist;

    Track():
        Record { "tracks" },
        title { *this },
        artist { *this }
    {
    }
};

TEST_CASE_METHOD(SqlModelTestFixture, "Model.BelongsTo", "[model]")
{
    REQUIRE(Artist::CreateTable());
    REQUIRE(Track::CreateTable());

    Artist artist;
    artist.name = "Snoop Dog";
    REQUIRE(artist.Save());

    Track track1;
    track1.title = "Wuff";
    track1.artist = artist;
    REQUIRE(track1.Save());

    Track track2;
    track2.title = "Paff Dog";
    track2.artist = artist;
    REQUIRE(track2.Save());

    REQUIRE(artist.tracks.IsLoaded() == false);
    REQUIRE(artist.tracks.IsEmpty().value() == false);
    REQUIRE(artist.tracks.Count() == 2);
    // TODO: REQUIRE(artist.tracks.Load());
    // TODO: REQUIRE(artist.tracks.IsLoaded() == true);
}
