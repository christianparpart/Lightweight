#include "../JPSql/Model.hpp"
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

    Track():
        Record { "tracks" },
        title { *this },
        artist { *this }
    {
    }

    Track(Track&& other) noexcept:
        Record { std::move(other) },
        title { *this, std::move(other.title) },
        artist { *this, std::move(other.artist) }
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
    track1.artist = artist; // track1 "BelongsTo" artist
    REQUIRE(track1.Save());

    CHECK(track1.artist->Inspect() == artist.Inspect());
}

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
    struct Suppliers;
    struct Account;

    struct Suppliers: Model::Record<Suppliers>
    {
        Model::Field<std::string, 2, "name"> name;
        Model::HasOne<Account, "supplier_id"> account;

        Suppliers():
            Record { "suppliers" },
            name { *this },
            account { *this }
        {
        }

        Suppliers(Suppliers&& other) noexcept:
            Record { std::move(other) },
            name { *this, std::move(other.name) },
            account { *this, std::move(other.account) }
        {
        }
    };

    struct Account: Model::Record<Account>
    {
        Model::Field<std::string, 2, "iban"> iban;
        Model::BelongsTo<Suppliers, 3, "supplier_id"> supplier;

        Account():
            Record { "accounts" },
            iban { *this },
            supplier { *this }
        {
        }

        Account(Account&& other) noexcept:
            Record { std::move(other) },
            iban { *this, std::move(other.iban) },
            supplier { *this, std::move(other.supplier) }
        {
        }
    };

    REQUIRE(Suppliers::CreateTable());
    REQUIRE(Account::CreateTable());

    Suppliers supplier;
    supplier.name = "Supplier";
    REQUIRE(supplier.Save());

    Account account;
    account.iban = "DE123456789";
    account.supplier = supplier;
    REQUIRE(account.Save());

    REQUIRE(supplier.account.IsLoaded() == false);
    REQUIRE(supplier.account.Load());
    REQUIRE(supplier.account.IsLoaded() == true);
    REQUIRE(supplier.account->Inspect() == account.Inspect());
}
