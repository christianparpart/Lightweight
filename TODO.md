
### TODO items

- [x] reduce the need of SqlComposedQuery
- [x] Add named `LeftJoin`, `RightJoin`, `FullJoin` and `CrossJoin` to where-clause builder
- [ ] Add sub-query support to where-clause builder
- [ ] Have test for complex query builder use actually sending queries to the database
- [x] Add insert query builder
- [x] Add update query builder
- [ ] Better address boolean `Where` clauses wrt association priorities
- [ ] fix readme's qeury builder examples

- [x] check for more use of `ExecuteDirectScalar`
- [ ] replace `m_` with simple leading underscore `_` in member variables
- [ ] `SqlStatement::BindINputParameters<Params...>(Params... params)` to unfold the pack with index properly incremented from 1...
