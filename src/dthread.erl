%%% @author Tony Rogvall <tony@rogvall.se>
%%% @copyright (C) 2012, Tony Rogvall
%%% @doc
%%%    dthread test 
%%% @end
%%% Created :  6 May 2012 by Tony Rogvall <tony@rogvall.se>

-module(dthread).

-compile(export_all).


open() ->
    ok = erl_ddll:load_driver(code:priv_dir(dthread), "dthread_drv"),
    open_port({spawn_driver, dthread_drv}, [binary]).

close(Port) ->
    port_close(Port).

test_ctl(Port) ->
    port_control(Port, 1, "hello").

test_command(Port) ->
    port_command(Port, "Hello").


