
### TODO items

- [ ] eliminate the need of SqlComposedQuery
- [ ] -> then rename SqlComposedQuery.hpp to SqlQuery(Builder?).hpp (and maybe classes to Sql...Query ?)
- [x] check for more use of `ExecuteDirectScalar`
- [ ] Add named `LeftJoin`, `RightJoin`, `FullJoin` and `CrossJoin` to where-clause builder
- [ ] Add sub-query support to where-clause builder
- [x] Add insert query builder
- [ ] Add update query builder
- [ ] replace `m_` with simple leading underscore `_` in member variables
- [ ] `SqlStatement::BindINputParameters<Params...>(Params... params)` to unfold the pack with index properly incremented from 1...
