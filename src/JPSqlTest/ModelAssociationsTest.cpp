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

    Artist(Artist&& other) noexcept:
        Record { std::move(other) },
        name { *this, std::move(other.name) },
        tracks { *this, std::move(other.tracks) }
    {
    }
};

struct Track: Model::Record<Track>
{
    Model::Field<std::string, 2, "title"> title;
    Model::BelongsTo<Artist, 3, "artist_id"> artist;
    //Model::HasOne<Artist, 4, "featured_artist_id"> featuredArtist;

    Track():
        Record { "tracks" },
        title { *this },
        artist { *this }
        // , featuredArtist { *this }
    {
    }

    Track(Track&& other) noexcept:
        Record { std::move(other) },
        title { *this, std::move(other.title) },
        artist { *this, std::move(other.artist) }
        //, featuredArtist { *this, std::move(other.featuredArtist) }
    {
    }
};

TEST_CASE_METHOD(SqlModelTestFixture, "Model.HasMany", "[model]")
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
    REQUIRE(artist.tracks.Load());
    REQUIRE(artist.tracks.IsLoaded() == true);
    REQUIRE(artist.tracks.Count() == 2); // Using cached value
    REQUIRE(artist.tracks[0].Inspect() == track1.Inspect());
    REQUIRE(artist.tracks[1].Inspect() == track2.Inspect());
}

TEST_CASE_METHOD(SqlModelTestFixture, "Model.HasOne", "[model]")
{
    REQUIRE(Artist::CreateTable());
    REQUIRE(Track::CreateTable());

    Artist artist;
    artist.name = "Snoop Dog";
    REQUIRE(artist.Save());

    Artist featuredArtist;
    featuredArtist.name = "Snoop Dog";
    REQUIRE(featuredArtist.Save());

    // Track track1;
    // track1.title = "Wuff";
    // track1.artist = artist;
    // track1.featuredArtist = featuredArtist;
    // REQUIRE(track1.Save());

    // REQUIRE(track1.featuredArtist.IsLoaded() == false);
    // REQUIRE(track1.featuredArtist->Inspect() == featuredArtist.Inspect());
    // REQUIRE(track1.featuredArtist.IsLoaded() == true);
}
