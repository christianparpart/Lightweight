#include "../JPSql/Model/Logger.hpp"
#include "../JPSql/Model/Record.hpp"
#include "../JPSql/Model/Relation.hpp"
#include "../JPSql/Model/Utils.hpp"
#include "../JPSql/SqlConnection.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

struct Appointment;
struct Patient;

struct Doctor: Model::Record<Doctor>
{
    Model::Field<std::string, 2, "name"> name;
    Model::HasMany<Appointment, "doctor_id"> appointments;

    Doctor():
        Record { "doctors" },
        name { *this },
        appointments { *this }
    {
    }

    // Model::HasManyThrough<Patient, Appointment, "doctor_id", "patient_id"> patients;
    //
    // This translates to the following JOIN query:
    // SELECT patients.* FROM patients JOIN appointments ON patients.id = appointments.patient_id WHERE
    // appointments.doctor_id = ? Or: SELECT * FROM patients WHERE id IN (SELECT patient_id FROM appointments WHERE
    // doctor_id = ?)
};

struct Appointment: Model::Record<Appointment>
{
    Model::Field<SqlDateTime, 3, "when"> when;
    Model::BelongsTo<Patient, 4, "patient_id"> patient;

    Appointment():
        Record { "appointments" },
        when { *this },
        patient { *this }
    {
    }
};

struct Patient: Model::Record<Patient>
{
    Model::Field<std::string, 2, "name"> name;
    Model::HasMany<Appointment, "patient_id"> appointments;

    Patient():
        Record { "patients" },
        name { *this },
        appointments { *this }
    {
    }
};

TEST_CASE_METHOD(SqlModelTestFixture, "Model.BelongsTo", "[model]")
{
    REQUIRE(Doctor::CreateTable());
    REQUIRE(Appointment::CreateTable());
    REQUIRE(Patient::CreateTable());

    Doctor doctor;
    doctor.name = "Dr. House";
    REQUIRE(doctor.Save());

    Patient patient;
    patient.name = "John Doe";
    REQUIRE(patient.Save());

    Appointment appointment;
    appointment.when = SqlDateTime::Now();
    appointment.patient = patient.Id();
    REQUIRE(appointment.Save());

//    auto const fetchedAppointment = Appointment::Find(appointment.Id());
//    REQUIRE(fetchedAppointment);
//    REQUIRE(fetchedAppointment->patient == patient.Id());
}
