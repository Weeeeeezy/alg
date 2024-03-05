from os.path import join
import os

import sys
import numpy as np
import pandas as pd
from clickhouse_driver import Client


def load_synchots(ticker, exchange, start_ts, end_ts, folderpath):
    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_syncshots.csv")
    if os.path.exists(filepath):
        return

    start_dt_str = start_dt.strftime('%Y-%m-%d %H:%M:%S')
    end_dt_str = end_dt.strftime('%Y-%m-%d %H:%M:%S')
    settings = {'receive_timeout': 2400}

    client = Client.from_url('clickhouse://dataflow:kdhrtBS736svdgG5s@click.covexity.com:9000/md2')
    client.settings = settings
    res_sync_orderbook = client.execute(f"""SELECT client_ts, client_ts_nanos, op, price, qty, side
                                             from order_book
                                             where ticker = '{ticker}' and exchange = '{exchange}'
                                             and (op = 'syncshot')
                                             and client_ts >= '{start_dt_str}' and client_ts < '{end_dt_str}'""")

    sync_df = pd.DataFrame(res_sync_orderbook, columns=['client_ts', 'client_ts_nanos', 'op', 'price', 'qty', 'side'])
    sync_df = sync_df.drop('op', axis=1)

    sync_df['timestamp'] = sync_df.client_ts.astype(int) + sync_df.client_ts_nanos
    sync_df = sync_df.sort_values('timestamp')
    sync_df['side_int'] = np.where(sync_df.side == 'buy', 1, -1)

    sync_df = sync_df[['timestamp', 'side_int', 'price', 'qty']]
    sync_df.to_csv(filepath, index=False, header=False, sep=",")


def load_trades(ticker, exchange, start_ts, end_ts, folderpath):
    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_trades.csv")
    if os.path.exists(filepath):
        return

    start_dt_str = start_dt.strftime('%Y-%m-%d %H:%M:%S')
    end_dt_str = end_dt.strftime('%Y-%m-%d %H:%M:%S')

    settings = {'receive_timeout': 2400}
    client = Client.from_url('clickhouse://dataflow:kdhrtBS736svdgG5s@click.covexity.com:9000/md2')
    client.settings = settings
    res_trades = client.execute(f"""SELECT client_ts, client_ts_nanos, price, abs(qty) qty, side_int
                                      from taq
                                      where type = 'trade' and ticker = '{ticker}' and exchange = '{exchange}'
                                      and client_ts >= '{start_dt_str}' and client_ts < '{end_dt_str}'""")

    trades = pd.DataFrame(res_trades, columns=['client_ts', 'client_ts_nanos', 'price', 'qty', 'side_int'])
    trades['timestamp'] = trades.client_ts.astype(int) + trades.client_ts_nanos
    trades = trades.sort_values('timestamp')
    trades = trades[['timestamp', 'side_int', 'price', 'qty']]
    trades.to_csv(filepath, index=False, header=False, sep=",")


def load_updates(ticker, exchange, start_ts, end_ts, folderpath):
    start_dt = pd.to_datetime(start_ts)
    end_dt = pd.to_datetime(end_ts)
    filepath = join(folderpath, f"{ticker}_{exchange}_{start_dt.strftime('%Y%m%d%H%M%S')}_{end_dt.strftime('%Y%m%d%H%M%S')}_updates.csv")
    if os.path.exists(filepath):
        return

    start_dt_str = start_dt.strftime('%Y-%m-%d %H:%M:%S')
    end_dt_str = end_dt.strftime('%Y-%m-%d %H:%M:%S')

    settings = {'receive_timeout': 2400}
    client = Client.from_url('clickhouse://dataflow:kdhrtBS736svdgG5s@click.covexity.com:9000/md2')
    client.settings = settings
    res_orderbook = client.execute(f"""SELECT client_ts, client_ts_nanos, op, price, qty, side
                                              from order_book
                                              where ticker = '{ticker}' and exchange = '{exchange}' and op = 'update'
                                              and client_ts >= '{start_dt_str}' and client_ts < '{end_dt_str}'""")

    upd_df = pd.DataFrame(res_orderbook, columns=['client_ts', 'client_ts_nanos', 'op', 'price', 'qty', 'side'])
    upd_df = upd_df.drop('op', axis=1)
    upd_df['timestamp'] = upd_df.client_ts.astype(int) + upd_df.client_ts_nanos
    upd_df = upd_df.sort_values('timestamp')
    upd_df['side_int'] = -1
    upd_df['side_int'] = np.where(upd_df.side == 'buy', 1, -1)
    upd_df = upd_df[['timestamp', 'side_int', 'price', 'qty']]
    upd_df.to_csv(filepath, index=False, header=False, sep=",")


if __name__ == "__main__":
    ticker = sys.argv[1]
    exchange = sys.argv[2]
    start_ts = sys.argv[3]
    end_ts = sys.argv[4]
    folderpath = os.getcwd()

    load_trades(ticker, exchange, start_ts, end_ts, folderpath)
    load_updates(ticker, exchange, start_ts, end_ts, folderpath)
    load_synchots(ticker, exchange, start_ts, end_ts, folderpath)

