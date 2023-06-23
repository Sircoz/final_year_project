#from pyvelox.pyvelox import ITypedExprModifier as ModifExpr
import pyvelox.pyvelox as pv
import time
import statistics
import random
import numpy

UNIFORM = 0
GAUSSIAN = 1

conjunctions = ["and"]
comparators = ["eq", "neq", "gte", "lte", "gt", "lt"]
labelConstantMutation = {'age': GAUSSIAN,
                        'workclass': UNIFORM,
                        'education': UNIFORM,
                        'educationnum': GAUSSIAN,
                        'maritalstatus': UNIFORM,
                        'occupation': UNIFORM,
                        'relationship': UNIFORM,
                        'race': UNIFORM,
                        'sex': UNIFORM,
                        'capitalgain': GAUSSIAN,
                        'capitalloss': GAUSSIAN,
                        'hoursperweek': GAUSSIAN,
                        'nativecountry': UNIFORM}

min = {'age': 21,
       'workclass': 0,
       'education': 0,
       'educationnum': 0,
       'maritalstatus': 0,
       'occupation': 0,
       'relationship': 0,
       'race': 0,
       'sex': 0,
       'capitalgain': 0,
       'capitalloss': 0,
       'hoursperweek': 0,
       'nativecountry': 0}
    
max = {'age': 73,
       'workclass': 8,
       'education': 15,
       'educationnum': 15,
       'maritalstatus': 6,
       'occupation': 14,
       'relationship': 5,
       'race': 4,
       'sex': 1,
       'capitalgain': 122,
       'capitalloss': 98,
       'hoursperweek': 95,
       'nativecountry': 41}

def sampleUniform(label):
    return int(random.uniform(min[label], max[label]))

def sampleGaussian(label, value):
    ret = int(numpy.random.normal(value, 10))
    if ret < min[label]:
        return min[label]
    if ret > max[label]:
        return max[label]
    return ret

def applyMutations(expr):
    inputs = []
    label = ""

    for input in expr.getInputs():
        if input.isFieldAccessExpr():
            label = input.toFieldAccessExpr().getName()

    for input in expr.getInputs():
        if bool(random.getrandbits(1)) and input.isConstantExpr() and label != "" and label in list(labelConstantMutation.keys()):
            const = input.toConstantExpr()
            if labelConstantMutation[label] == UNIFORM:
                inputs.append(const.mutate(sampleUniform(label)))
            elif labelConstantMutation[label] == GAUSSIAN:
                inputs.append(const.mutate(sampleGaussian(label, const.getValue())))
            else:
                inputs.append(input)
        else:
            inputs.append(applyMutations(input))

    if bool(random.getrandbits(1)) and expr.isCallExpr():
        call = expr.toCallExpr()
        name = call.getName()
        if name in conjunctions:
            return call.mutate(random.choice(conjunctions), inputs)
        if name in comparators:
            return call.mutate(random.choice(comparators), inputs)
        
    return expr.copyWInputs(inputs)

def tryMutation():
    proj = pv.Project.create()
    proj.loadData("adult/discrete.csv")
    proj.setNumThreads(1)
    proj.setMaxDrivers(1)

    exprs = proj.compileQueriesFromFile("adult/queries_1000.txt")
    expr = exprs[2]
    print(expr.getSQL())
    mutant = applyMutations(expr)
    print(mutant.getSQL())

def runMutationEpochs(nbEpochs):
    reslist = []
    file = open('adult/pyvelox_mutation_test.txt','w') 

    for _ in range(0, 2):
        proj = pv.Project.create()
        proj.loadData("adult/discrete.csv")
        proj.setNumThreads(1)
        proj.setMaxDrivers(1)

        totTime = 0
        nbQueries = 0

        queries = []

        with open("ault/queries_1000.txt", 'r') as f:
            for line in f:
                line = line.strip()
                queries.append(line)

        st = time.time()
        exprs = proj.compileSQLQueries(queries)
        for expr in exprs:
            proj.runQueryOneThread(expr, 0)
        et = time.time()

        totTime += et-st
        nbQueries+=len(exprs)
        
        for epoch in range(0, nbEpochs):
            queries = []
            for q in range(0, 1000):
                st = time.time()
                mutant = applyMutations(exprs[q])
                et = time.time()
                queries.append(mutant)
                file.write(mutant.getSQL() + '\n')
                totTime += et-st
                nbQueries+=1
            st = time.time()
            for expr in queries:
                proj.runQueryOneThread(expr, 0)
            et = time.time()
            totTime += et-st

        reslist.append(totTime * 1000 / nbQueries)
    
    print('1:Execution time mean python mutation:', statistics.mean(reslist), 'milliseconds')
    print('1:Execution time std python mutation:', statistics.stdev(reslist), 'milliseconds')

def runEasyMutationEpochs(nbEpochs):
    reslist = []
    file = open('adult/pyvelox_mutation_test.txt','w') 

    for _ in range(0, 2):
        proj = pv.Project.create()
        proj.loadData("adult/discrete.csv")
        proj.setNumThreads(1)
        proj.setMaxDrivers(1)

        proj.addModifyCalls(set(conjunctions))
        proj.addModifyCalls(set(comparators))
        for key, value in labelConstantMutation.items():
            if value == GAUSSIAN:
                proj.addModifyConstantGaussian(key, min[key], max[key], 10)
            elif value == UNIFORM:
                proj.addModifyConstantUniform(key, min[key], max[key])

        totTime = 0
        nbQueries = 0
        queries = []

        with open("ault/queries_1000.txt", 'r') as f:
            for line in f:
                line = line.strip()
                queries.append(line)

        st = time.time()
        exprs = proj.compileSQLQueries(queries)
        for expr in exprs:
            proj.runQueryOneThread(expr, 0)
        et = time.time()

        totTime += et-st
        nbQueries+=len(exprs)
        
        for epoch in range(0, nbEpochs):
            queries = []
            for q in range(0, 1000):
                st = time.time()
                expr = proj.mutate(exprs[q])
                et = time.time()
                queries.append(expr)
                file.write(expr.getSQL() + '\n')
                totTime += et-st
                nbQueries+=1
        st = time.time()
        for expr in queries:
            proj.runQueryOneThread(expr, 0)
        et = time.time()
        totTime += et-st

        reslist.append(totTime * 1000 / nbQueries)
    
    print('3:Execution time mean C++ mutation:', statistics.mean(reslist), 'milliseconds')
    print('3:Execution time std  C++ mutation:', statistics.stdev(reslist), 'milliseconds')

def compileAll():
    reslist = []
    for k in range(0, 2):
        proj = pv.Project.create()
        proj.loadData("adult/discrete.csv")
        proj.setNumThreads(1)
        proj.setMaxDrivers(1)

        totTime = 0
        nbQueries = 0
        queries = []

        with open("ault/queries_1000.txt", 'r') as f:
            for line in f:
                line = line.strip()
                queries.append(line)

        queries = queries[:100]
        st = time.time()
        exprs = proj.compileSQLQueries(queries)
        
        for expr in exprs:
            result=proj.runQueryOneThread(expr, 0)
        et = time.time()

        totTime += et-st
        nbQueries+=len(exprs)

        with open("adult/pyvelox_mutation_test.txt") as file_in:
            lines = []
            for line in file_in:
                lines.append(line)
                if len(lines) == 1000:
                    st = time.time()
                    exprs = proj.compileFilters(lines)
                    for expr in exprs:
                        result=proj.runQueryOneThread(expr, 0)
                    et = time.time()
                    nbQueries += 1000
                    totTime += et-st
                    lines = []

        reslist.append(totTime * 1000 / nbQueries)

    
    print('2:Execution time mean DuckDB compilation:', statistics.mean(reslist), 'milliseconds')
    print('2:Execution time std DuckDB compilation:', statistics.stdev(reslist), 'milliseconds')


nbEpoch = int(input("Epochs: "))
runMutationEpochs(nbEpoch)
compileAll()
runEasyMutationEpochs(nbEpoch)