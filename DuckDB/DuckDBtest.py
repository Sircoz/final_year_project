import duckdb
import statistics
import time

def main():
    con = duckdb.connect(database=':memory:')
    con.execute('CREATE TABLE data AS SELECT * FROM read_csv_auto(\'adult/discrete.csv\')')
    con.execute('SET threads TO 1')

    queries = []
    

    with open('../../adult/queries_1000.txt', 'r') as f:
        for line in f:
            line = line.strip()

    times = []
    for i in range(0, 5):
        results = []

        start_time = time.time()
        for query in queries:
            results.append(con.execute(query).fetchall())
        end_time = time.time()
        res = end_time - start_time
        times.append(res * 1000)


    print('mean execution: ', statistics.mean(times))
    print('std execution: ', statistics.stdev(times))
    con.close()


if __name__ == "__main__":
    main()