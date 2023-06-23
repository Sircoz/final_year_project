import pyvelox.pyvelox as pv
import time
import statistics

def compileQueries():
    reslist = []

    for i in range(0,10):
        proj = pv.Project.create()
        proj.loadData("adult/discrete.csv")
        proj.setNumThreads(8)
        proj.setMaxDrivers(1)

        st = time.time()
        exprs = proj.compileQueriesFromFile("adult/queries_1000.txt")
        results = proj.runQueries(exprs, {})
        et = time.time()


        print(len(results[0]))
        res = et - st
        final_res = res * 1000
        reslist.append(final_res)

    print('Execution time mean:', statistics.mean(reslist), 'milliseconds')
    print('Execution time std:', statistics.stdev(reslist), 'milliseconds')


def compileQueriesOne():
    reslist = []

    proj = pv.Project.create()
    proj.loadData("adult/discrete.csv")
    proj.setNumThreads(1)
    proj.setMaxDrivers(1)


    for i in range(0,1):
        results = []

        exprs = proj.compileQueriesFromFile("adult/queries_1000.txt")
        st = time.time()
        for expr in exprs:
            results.append(proj.runQueryOneThread(expr, 0))
        et = time.time()
  
        res = et - st
        final_res = res * 1000
        reslist.append(final_res)
        print(results[0][0])

    print('Execution time mean:', statistics.mean(reslist), 'milliseconds')
    print('Execution time std:', statistics.stdev(reslist), 'milliseconds')

def testMultipleDBs():
    proj = pv.Project.create()
    proj.loadData("adult/discrete.csv")
    proj.loadData("adult/discrete.csv")
    proj.setNumThreads(4)
    proj.setMaxDrivers(1)

    exprs = proj.compileQueriesFromFile("adult/queries_1000.txt")
    results = proj.runQueries(exprs, {0: [0]})
    print(len(results))

compileQueries()