select distinct
  s.c_secid,
  a.c_name,
  b.c_name,
  s.c_precisionamount,
  s.c_mintokensinorder,
  s.c_precisionprice
from
  (select   c_secid,  c_precisionamount, c_mintokensinorder, c_precisionprice,
            c_tcurid, c_bcurid
   from  t_security
   where c_istradable <> 0
   group by c_secid,  c_precisionamount, c_mintokensinorder, c_precisionprice,
            c_tcurid, c_bcurid, c_timestamp
   having   c_timestamp  =  max(c_timestamp)
   order by c_secid) as s,
  (select distinct c_currid, c_name from t_currency order by c_currid) as a,
  (select distinct c_currid, c_name from t_currency order by c_currid) as b
where
  s.c_tcurid=a.c_currid and
  s.c_bcurid=b.c_currid
order by s.c_secid;
