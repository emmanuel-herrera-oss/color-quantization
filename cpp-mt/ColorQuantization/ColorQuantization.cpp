#include <iostream>
#include "lodepng.h"
#include <array>
#include <vector>
#include <math.h>
#include <chrono>
#include <thread>
#include "CLI11.hpp"

struct QuantizeOptions {
    unsigned char* image;
    unsigned int image_len;
    unsigned int cluster_count;
    unsigned int iteration_limit;
    unsigned int thread_count;
};

static unsigned int kmeans(const QuantizeOptions& options);

int main(int argc, char** argv) {
    // Options
    CLI::App app{ "This program maps the colors in a png image to a lower number of colors using the k-means algorithm." };
    argv = app.ensure_utf8(argv);
    std::string from, to;
    unsigned int cluster_count, max_iter, thread_count;
    app.add_option("-f,--from", from, "Source image path")->required();
    app.add_option("-t,--to", to, "Result image path")->required();
    app.add_option("-k,--clusters", cluster_count, "Number of clusters to use in k-means")->default_val(8);
    app.add_option("-i,--max_iter", max_iter, "Maximum number of k-means iterations.")->default_val(1000);
    app.add_option("-p,--threads", thread_count, "Number of threads")->default_val(4);
    CLI11_PARSE(app, argc, argv);
    std::cout << "Cluster Count: " << cluster_count << std::endl;
    std::cout << "Thread Count: " << thread_count << std::endl;

    // Load image
    unsigned char* image;
    unsigned int w, h;
    std::cout << "Reading " << from << "...\n";
    lodepng_decode_file(&image, &w, &h, from.c_str(), LCT_RGBA, 8);

    auto options = QuantizeOptions{
        .image = image,
        .image_len = w * h,
        .cluster_count = cluster_count,
        .iteration_limit = max_iter,
        .thread_count = thread_count
    };

    // Process
    std::cout << "Clustering is starting...\n";
    auto start = std::chrono::high_resolution_clock::now();
    auto iterations = kmeans(options);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Finished clustering " << w * h << " pixels (" << iterations << " iterations) in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " milliseconds. Storing image...\n";

    // Store output
    lodepng_encode_file(to.c_str(), image, w, h, LCT_RGBA, 8);
    std::cout << "Finished. Saved result to: " << to << std::endl;
}

struct Centroid {
    unsigned int count;
    float r, g, b;
    float prev_r, prev_g, prev_b;
};

static void init_centroids(Centroid* centroids, const QuantizeOptions& options);
static bool update_centroids(Centroid* centroids, const int* clusters, const QuantizeOptions& options);
static float distance_from_centroid(const Centroid&, const unsigned char* point);

volatile bool* signals;
volatile bool finished = false;

static void chunk(unsigned int thread_id, const QuantizeOptions& options, Centroid* centroids, int* clusters) {
    while (true) {
        if (finished) {
            return;
        }
        if (!signals[thread_id]) {
            continue;
        }
        int points_per_thread = options.image_len / options.thread_count + 1;
        for (unsigned int point_idx = thread_id * points_per_thread; point_idx < (thread_id + 1) * points_per_thread; point_idx++)
        {
            if (point_idx >= options.image_len) {
                break;
            }
            float min_distance = -1;
            int min_idx = -1;
            auto point = options.image + 4 * point_idx;
            for (unsigned int centroid_idx = 0; centroid_idx < options.cluster_count; centroid_idx++)
            {
                auto distance = distance_from_centroid(centroids[centroid_idx], point);
                if (min_idx < 0 || distance < min_distance)
                {
                    min_distance = distance;
                    min_idx = centroid_idx;
                }
            }
            clusters[point_idx] = min_idx;
        }
        signals[thread_id] = false;
    }
}

static unsigned int kmeans(const QuantizeOptions& options) {
    // Allocations 
    auto clusters = new int[options.image_len];
    auto centroids = new Centroid[options.cluster_count];
    signals = new bool[options.thread_count];

    // Initialization
    srand(static_cast<unsigned int>(time(0)));
    init_centroids(centroids, options);
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < options.thread_count; i++) {
        signals[i] = false;
        threads.push_back(std::thread(chunk, i, options, centroids, clusters));
    }

    // k-means loop
    bool converged = false;
    unsigned int last_iteration;
    for (unsigned int iteration = 0; iteration < options.iteration_limit; iteration++)
    {
        for (unsigned int i = 0; i < options.thread_count; i++) {
            signals[i] = true;
        }
        while (true) {
            bool finished = true;
            for (unsigned int i = 0; i < options.thread_count; i++) {
                if (signals[i]) {
                    finished = false;
                }
            }
            if (finished) {
                break;
            }
        }
        bool anyChanged = update_centroids(centroids, clusters, options);
        if (!anyChanged)
        {
            last_iteration = iteration + 1;
            converged = true;
            break;
        }
    }
    if (!converged)
    {
        throw new std::runtime_error("Did not converge");
    }

    // end threads
    finished = true;
    for (auto& thread : threads) {
        thread.join();
    }

    // update every pixel with rgb from the centroid it was assigned to
    for (unsigned int i = 0; i < options.image_len; i++) {
        auto centroid = centroids[clusters[i]];
        auto point = options.image + 4 * i;
        point[0] = static_cast<unsigned char>(centroid.r);
        point[1] = static_cast<unsigned char>(centroid.g);
        point[2] = static_cast<unsigned char>(centroid.b);
    }
    return last_iteration;
}

static void init_centroids(Centroid* centroids, const QuantizeOptions& options) {
    for (unsigned int i = 0; i < options.cluster_count; i++)
    {
        auto randIndex = static_cast<int>(4 * (1.0f * std::rand() / RAND_MAX) * (options.image_len - 1));
        centroids[i] = Centroid{
            .r = static_cast<float>(options.image[randIndex]),
            .g = static_cast<float>(options.image[randIndex + 1]),
            .b = static_cast<float>(options.image[randIndex + 2])
        };
    }
}

static bool update_centroids(Centroid* centroids, const int* clusters, const QuantizeOptions& options) {
    const float threshold = 0.01f;
    for (unsigned int i = 0; i < options.cluster_count; i++) {
        centroids[i].prev_r = centroids[i].r;
        centroids[i].prev_g = centroids[i].g;
        centroids[i].prev_b = centroids[i].b;

        centroids[i].r = 0;
        centroids[i].g = 0;
        centroids[i].b = 0;

        centroids[i].count = 0;
    }

    for (unsigned int i = 0; i < options.image_len; i++) {
        auto point = options.image + 4 * i;
        auto centroid_idx = clusters[i];
        centroids[centroid_idx].r += point[0];
        centroids[centroid_idx].g += point[1];
        centroids[centroid_idx].b += point[2];
        centroids[centroid_idx].count += 1;
    }

    bool anyChanged = false;
    for (unsigned int i = 0; i < options.cluster_count; i++) {
        if (centroids[i].count > 0) {
            centroids[i].r /= centroids[i].count;
            centroids[i].g /= centroids[i].count;
            centroids[i].b /= centroids[i].count;
        }
        if (anyChanged) {
            continue;
        }
        if (
            (abs((centroids[i].r - centroids[i].prev_r) / centroids[i].prev_r) > threshold) ||
            (abs((centroids[i].g - centroids[i].prev_g) / centroids[i].prev_g) > threshold) ||
            (abs((centroids[i].b - centroids[i].prev_b) / centroids[i].prev_b) > threshold)
            ) {
            anyChanged = true;
        }
    }

    return anyChanged;
}

static float distance_from_centroid(const Centroid& centroid, const unsigned char* point)
{
    float r = point[0];
    float g = point[1];
    float b = point[2];

    return sqrt(
        (centroid.r - r) * (centroid.r - r) +
        (centroid.g - g) * (centroid.g - g) +
        (centroid.b - b) * (centroid.b - b));
}
