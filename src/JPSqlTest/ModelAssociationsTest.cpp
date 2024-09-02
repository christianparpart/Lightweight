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
    REQUIRE(artist.Id().value);

    Track track1;
    track1.title = "Wuff";
    track1.artist = artist; // track1 "BelongsTo" artist
    REQUIRE(track1.Save());
    REQUIRE(track1.Id().value);

    CHECK(track1.artist->Inspect() == artist.Inspect());

    REQUIRE(artist.Destroy());
    CHECK(Artist::Count() == 0);
    CHECK(Track::Count() == 0);
    // Destroying the artist must also destroy the track, due to the foreign key constraint.
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

TEST_CASE_METHOD(SqlModelTestFixture, "Model.HasOneThrough", "[model]")
{
    // {{{ models
    struct Suppliers;
    struct Account;
    struct AccountHistory;

    struct Suppliers: Model::Record<Suppliers>
    {
        Model::HasOne<Account, "supplier_id"> account;
        Model::HasOneThrough<AccountHistory, "account_id", Account> accountHistory;
        Model::Field<std::string, 2, "name"> name;

        // {{{ ctors
        Suppliers():
            Record { "suppliers" },
            account { *this },
            accountHistory { *this },
            name { *this }
        {
        }

        Suppliers(Suppliers&& other) noexcept:
            Record { std::move(other) },
            account { *this, std::move(other.account) },
            accountHistory { *this, std::move(other.accountHistory) },
            name { *this, std::move(other.name) }
        {
        }
        // }}}
    };

    struct Account: Model::Record<Account>
    {
        Model::Field<std::string, 2, "iban"> iban;
        Model::BelongsTo<Suppliers, 2, "supplier_id"> supplier;
        Model::HasOne<AccountHistory, "account_id"> accountHistory;

        // {{{ ctors
        Account():
            Record { "accounts" },
            iban { *this },
            supplier { *this },
            accountHistory { *this }
        {
        }

        Account(Account&& other) noexcept:
            Record { std::move(other) },
            iban { *this, std::move(other.iban) },
            supplier { *this, std::move(other.supplier) },
            accountHistory { *this, std::move(other.accountHistory) }
        {
        }
        // }}}
    };

    struct AccountHistory: Model::Record<AccountHistory>
    {
        Model::BelongsTo<Account, 2, "account_id"> account;
        Model::Field<std::string, 3, "description"> description;

        // {{{ ctors
        AccountHistory():
            Record { "account_histories" },
            account { *this },
            description { *this }
        {
        }

        AccountHistory(AccountHistory&& other) noexcept:
            Record { std::move(other) },
            account { *this, std::move(other.account) },
            description { *this, std::move(other.description) }
        {
        }
        // }}}
    };
    // }}}

    REQUIRE(Suppliers::CreateTable());
    REQUIRE(Account::CreateTable());
    REQUIRE(AccountHistory::CreateTable());

    Suppliers supplier;
    supplier.name = "The Supplier";
    supplier.Save();

    Account account;
    account.supplier = supplier;
    account.iban = "DE123456789";
    account.Save();

    AccountHistory accountHistory;
    accountHistory.account = account;
    accountHistory.description = "Initial deposit";
    accountHistory.Save();

    REQUIRE(supplier.accountHistory.IsLoaded() == false);
    REQUIRE(supplier.accountHistory->Inspect() == accountHistory.Inspect()); // auto-loads the accountHistory
    REQUIRE(supplier.accountHistory.IsLoaded() == true);
}

TEST_CASE_METHOD(SqlModelTestFixture, "Model.HasManyThrough", "[model]")
{
    // {{{ Models
    struct Physician;
    struct Appointment;
    struct Patient;

    struct Physician: Model::Record<Physician>
    {
        Model::Field<std::string, 2, "name"> name;
        Model::HasMany<Appointment, "physician_id"> appointments;
        Model::HasManyThrough<Patient, "patient_id", Appointment, "physician_id"> patients;

        Physician():
            Record { "physicians" },
            name { *this },
            appointments { *this },
            patients { *this }
        {
        }

        Physician(Physician&& other) noexcept:
            Record { std::move(other) },
            name { *this, std::move(other.name) },
            appointments { *this, std::move(other.appointments) },
            patients { *this, std::move(other.patients) }
        {
        }
    };

    struct Appointment: Model::Record<Appointment>
    {
        Model::Field<SqlDateTime, 2, "date"> date;
        Model::Field<std::string, 3, "comment"> comment;
        Model::BelongsTo<Physician, 4, "physician_id"> physician;
        Model::BelongsTo<Patient, 5, "patient_id"> patient;

        Appointment():
            Record { "appointments" },
            date { *this },
            comment { *this },
            physician { *this },
            patient { *this }
        {
        }

        Appointment(Appointment&& other) noexcept:
            Record { std::move(other) },
            date { *this, std::move(other.date) },
            comment { *this, std::move(other.comment) },
            physician { *this, std::move(other.physician) },
            patient { *this, std::move(other.patient) }
        {
        }
    };

    struct Patient: Model::Record<Patient>
    {
        Model::Field<std::string, 2, "name"> name;
        Model::Field<std::string, 3, "comment"> comment;
        Model::HasMany<Appointment, "patient_id"> appointments;
        Model::HasManyThrough<Physician, "physician_id", Appointment, "patient_id"> physicians;

        Patient():
            Record { "patients" },
            name { *this },
            comment { *this },
            appointments { *this },
            physicians { *this }
        {
        }

        Patient(Patient&& other) noexcept:
            Record { std::move(other) },
            name { *this, std::move(other.name) },
            comment { *this, std::move(other.comment) },
            appointments { *this, std::move(other.appointments) },
            physicians { *this, std::move(other.physicians) }
        {
        }
    };
    // }}}

    REQUIRE(Physician::CreateTable());
    REQUIRE(Patient::CreateTable());
    REQUIRE(Appointment::CreateTable());

    Physician physician1;
    physician1.name = "Dr. House";
    REQUIRE(physician1.Save());

    Physician physician2;
    physician2.name = "Granny";
    REQUIRE(physician2.Save());

    Patient patient1;
    patient1.name = "Blooper";
    patient1.comment = "Prefers morning times";
    REQUIRE(patient1.Save());

    Patient patient2;
    patient2.name = "Valentine";
    patient2.comment = "allways friendly";
    REQUIRE(patient2.Save());

    Appointment patient1Apointment1;
    patient1Apointment1.date = SqlDateTime::Now();
    patient1Apointment1.patient = patient1;
    patient1Apointment1.physician = physician2;
    patient1Apointment1.comment = "Patient is a bit nervous";
    REQUIRE(patient1Apointment1.Save());

    Appointment patient1Apointment2;
    patient1Apointment2.date = SqlDateTime::Now();
    patient1Apointment2.patient = patient2;
    patient1Apointment2.physician = physician2;
    patient1Apointment2.comment = "Patient is a bit nervous, again";
    REQUIRE(patient1Apointment2.Save());

    Appointment patient2Apointment1;
    patient2Apointment1.date = SqlDateTime::Now();
    patient2Apointment1.patient = patient2;
    patient2Apointment1.physician = physician1;
    patient2Apointment1.comment = "Patient is funny";
    REQUIRE(patient2Apointment1.Save());

    auto const& physician2Patients = physician2.patients.All();
    REQUIRE(physician2Patients.size() == 2);
    CHECK(physician2Patients.at(0).Inspect() == patient1.Inspect());
    CHECK(physician2Patients.at(1).Inspect() == patient2.Inspect());
}
