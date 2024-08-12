#pragma once

// Skatching the surface of a modern C++23 Object Relational Mapping (ORM) layer
//
// We usually know exactly the schema of the database we are working with,
// but we need to be able to address columns with an unknown name but known index,
// and we need to be able to deal with tables of unknown column count.

// TODO: [ ] Add std::format support for: SqlModel<T>
// TODO: [ ] Differenciate between VARCHAR (std::string) and TEXT (maybe SqlText<std::string>?)
// TODO: [x] Make logging more useful, adding payload data (similar to ActiveRecord)
// TODO: [x] remove debug prints
// TODO: [x] add proper trace logging (like ActiveRecord)

#include "Model/ModelId.hpp"
#include "Model/Field.hpp"
#include "Model/Record.hpp"
#include "Model/Relation.hpp"
