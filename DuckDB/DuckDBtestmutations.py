import duckdb
import statistics
import time

def main():
    nbQueries = 100
    con = duckdb.connect(database=':memory:')
    con.execute('CREATE TABLE data AS SELECT * FROM read_csv_auto(\'adult/discrete.csv\')')
    con.execute('SET threads TO 1')

    queries = []
    
    with open('DuckDB/adult/queries_' + str(nbQueries) + '.txt', 'r') as f:
            for line in f:
                line = line.strip()
                queries.append(line)

    times = []
    results = []
    for i in range(0, 2):
        nbQueries = 0
        res = 0
        
        start_time = time.time()
        for query in queries:
            results.append(con.execute(query).fetchall())
            nbQueries +=1
        end_time = time.time()
        res += (end_time - start_time) * 1000
        with open("adult/pyvelox_mutation_test.txt") as file_in:
            j=0
            start_time = time.time()
            for line in file_in:  
                results.append(con.execute("SELECT id FROM data WHERE " + line).fetchall())
                j+=1
                nbQueries += 1
                if j==900:
                     break
            end_time = time.time()
            res += (end_time - start_time) * 1000
            
        
        times.append(res / nbQueries)


    print('mean execution: ', statistics.mean(times))
    print('std execution: ', statistics.stdev(times))
    con.close()


if __name__ == "__main__":
    main()