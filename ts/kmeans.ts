/**
 * @param input Array of Vectors. Last element in each vector is an optional identifier. 
 * @param numberOfClusters 'k' in 'k-means' clustering
 * @param iterationLimit Stop if not converged after this many iterations
 */
export function kmeans(
        input: number[][], 
        numberOfClusters: number, 
        iterationLimit: number) {
    let clusters: number[][][] = []
    let centroids: number[][]

    centroids = initCentroids(input, numberOfClusters)
    let converged = false
    let iterationCount = 0
    for (let i = 0; i < iterationLimit; i++) {
        iterationCount++
        clusters = initClusters(numberOfClusters)
        for (let j = 0; j < input.length; j++) {
            const point = input[j]
            let min = -1
            let minIdx = -1
            for (let k = 0; k < centroids.length; k++) {
                const d = vectorDistance(point, centroids[k])
                if (min == -1 || d < min) {
                    min = d
                    minIdx = k
                }
            }
            clusters[minIdx].push(point)
        }
        const anyChanged = updateCentroids(centroids, clusters)
        if (!anyChanged) {
            converged = true
            break
        }
    }
    return { 
        centroids, 
        clusters, 
        iterationCount, 
        converged
    }
}

// Initialize k centroids from k random samples of the input data
function initCentroids(input: number[][], numberOfClusters: number) {
    const centroids: number[][] = new Array(numberOfClusters)
    for (let i = 0; i < numberOfClusters; i++) {
        while(!centroids[i] || centroids.findIndex((c, j) => c && i != j && vectorEquals(c, centroids[i])) > -1) {
            const randIndex = Math.floor(Math.random() * input.length)
            centroids[i] = input[randIndex]
        }
    }
    return centroids
}

// Return k centroids that are the average of the vectors in each cluster
function updateCentroids(centroids: number[][], clusters: number[][][]) {
    const threshold = 0.01
    let anyChanged = false
    for(let i = 0;i < centroids.length;i++) {
        const oldR = centroids[i][0]
        const oldG = centroids[i][1]
        const oldB = centroids[i][2]

        let newR = 0
        let newG = 0
        let newB = 0

        for(let j = 0;j < clusters[i].length;j++) {
            newR += clusters[i][j][0]
            newG += clusters[i][j][1]
            newB += clusters[i][j][2]
        }
        if(clusters[i].length > 0) {
            newR /= clusters[i].length
            newG /= clusters[i].length
            newB /= clusters[i].length
        }
        centroids[i][0] = newR
        centroids[i][1] = newG
        centroids[i][2] = newB

        if(anyChanged || 
            Math.abs((newR - oldR) / oldR) > threshold ||
            Math.abs((newG - oldG) / oldG) > threshold ||
            Math.abs((newB - oldB) / oldB) > threshold ) {
            anyChanged = true
        }
    }
    return anyChanged
}
function initClusters(numberOfClusters: number) {
    const clusters: number[][][] = new Array(numberOfClusters)
    for (let i = 0; i < numberOfClusters; i++) {
        if(clusters[i] === undefined) {
            clusters[i] = []
        }
        else {
            clusters[i] = []
        }
    }
    return clusters
}
function vectorEquals(v1: number[], v2: number[]) {
    return (
        v1[0] == v2[0] &&
        v1[1] == v2[1] &&
        v1[2] == v2[2]
    )
}
function vectorDistance(v1: number[], v2: number[]) {
    const sum = 
        (v1[0] - v2[0]) * (v1[0] - v2[0]) + 
        (v1[1] - v2[1]) * (v1[1] - v2[1]) + 
        (v1[2] - v2[2]) * (v1[2] - v2[2])
    return sum
}