-- Ada is a structured, statically typed, imperative, wide-spectrum, and object-oriented high-level computer programming language, extended from Pascal and other languages.
-- It has built-in language support for design-by-contract,
-- extremely strong typing,
-- explicit concurrency,
-- offering tasks,
-- synchronous message passing,
-- protected objects,
-- and non-determinism.
--
-- Ada was originally designed by a team led by Jean Ichbiah of CII Honeywell
-- Bull under contract to the United States Department of Defense (DoD) from
-- 1977 to 1983 to supersede the hundreds of programming languages then used by
-- the DoD.
-- Ada was named after Ada Lovelace (1815-1852), who is credited with being the first computer programmer.
with Ada.Text_IO;
use Ada.Text_IO;

procedure Hello is
begin
  Put_Line( "hello, world" );
end Hello;
